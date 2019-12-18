
#include "kvsstore_pages.h"

template<typename Functor>
int KvsStoreDataObject::read(uint64_t offset, uint64_t len, bufferlist &bl, Functor &&page_loader) {
    FTRACE
    const auto start = offset;
    const auto end = offset + len;
    auto remaining = len;

    TR << "prepare range";

    KvsPageSet::page_vector tls_pages;
    bool suc = data.get_range(offset, len, tls_pages, page_loader);

    if (!suc) {
        TR << "read failed - tls pages clear";
        tls_pages.clear();
        return -1;
    }

    TR << "range is ready: " << tls_pages.size();

    // allocate a buffer for the data
    buffer::ptr buf(len);

    auto p = tls_pages.begin();
    while (remaining) {

        // no more pages in range
        if (p == tls_pages.end() || (*p)->offset >= end) {
            buf.zero(offset - start, remaining);
            break;
        }

        auto page = *p;

        // read from page
        const auto page_offset = offset - page->offset;

        const auto count = std::min(remaining, /*data.get_page_size()*/ (uint64_t) page->length);

        buf.copy_in(offset - start, count, page->data + page_offset);

        remaining -= count;
        offset += count;

        ++p;
    }

    tls_pages.clear(); // drop page refs

    bl.append(std::move(buf));

    TR << "read done bl = " << ceph_str_hash_linux(bl.c_str(), bl.length()) << ", bl length = " << bl.length();
    return len;
}

template<typename Functor>
int KvsStoreDataObject::zero(uint64_t offset, unsigned len, Functor &&page_loader) {

    KvsPageSet::page_vector tls_pages;
    // make sure the page range is allocated
    data.alloc_range(offset, len, tls_pages, page_loader);

    auto page = tls_pages.begin();

    while (len > 0) {
        unsigned page_offset = offset - (*page)->offset;
        unsigned pageoff = data.get_page_size() - page_offset;
        unsigned count = std::min(len, pageoff);
        std::fill((*page)->data + page_offset,
                  (*page)->data + page_offset + count, 0);

        offset += count;
        len -= count;
        if (count == pageoff)
            ++page;
    }
    if (data_len < offset)
        data_len = offset;
    tls_pages.clear(); // drop page refs
    return 0;
}

template<typename Functor>
void KvsStoreDataObject::remove_object(uint64_t size, Functor &&page_remover) {
    KvsPageSet::page_vector tls_pages;
    data.alloc_range(0, size, tls_pages, [&](char *, int, uint32_t &) -> int {
        return 0;
    });

    for (const auto &p : tls_pages) {
        page_remover(p->offset);
    }

    data.free_pages_after(0);
}

template<typename Functor>
int KvsStoreDataObject::write(uint64_t offset, bufferlist &src, Functor &&page_loader) {
    unsigned len = src.length();

    KvsPageSet::page_vector tls_pages;
    // make sure the page range is allocated
    data.alloc_range(offset, src.length(), tls_pages, page_loader);


    //auto page = tls_pages.begin();

    auto p = src.begin();

    for (KvsPage *pg : tls_pages) {
        unsigned page_offset = offset - pg->offset;
        //TR << ", pg = " << (void*)pg << ", page offset " << page_offset << ", page length = " << pg->length << ", remaining " << len;
        p.copy(pg->length, pg->data + page_offset);
        offset += pg->length;
        len -= pg->length;
    }

    if (data_len < offset)
        data_len = offset;

    tls_pages.clear(); // drop page refs

    return 0;
}

template<typename Functor>
int KvsStoreDataObject::clone(KvsStoreDataObject *src, uint64_t srcoff,
                              unsigned len, uint64_t dstoff, Functor &&page_loader) {
    const int64_t delta = dstoff - srcoff;

    auto &src_data = static_cast<KvsStoreDataObject *>(src)->data;
    const uint64_t src_page_size = src_data.get_page_size();

    auto &dst_data = data;
    const auto dst_page_size = dst_data.get_page_size();

    KvsPageSet::page_vector tls_pages;
    KvsPageSet::page_vector dst_pages;


    while (len) {
        auto count = len;

        bool suc = src_data.get_range(srcoff, count, tls_pages, page_loader);
        if (!suc) {
            tls_pages.clear();
            return -1;
        }

        // allocate the destination range
        // TODO: avoid allocating pages for holes in the source range
        dst_data.alloc_range(srcoff + delta, count, dst_pages, page_loader);
        auto dst_iter = dst_pages.begin();

        for (auto &src_page : tls_pages) {
            auto sbegin = std::max(srcoff, src_page->offset);
            auto send = std::min(srcoff + count, src_page->offset + src_page_size);

            // zero-fill holes before src_page
            if (srcoff < sbegin) {
                while (dst_iter != dst_pages.end()) {
                    auto &dst_page = *dst_iter;
                    auto dbegin = std::max(srcoff + delta, dst_page->offset);
                    auto dend = std::min(sbegin + delta, dst_page->offset + dst_page_size);
                    std::fill(dst_page->data + dbegin - dst_page->offset,
                              dst_page->data + dend - dst_page->offset, 0);
                    if (dend < dst_page->offset + dst_page_size)
                        break;
                    ++dst_iter;
                }
                const auto c = sbegin - srcoff;
                count -= c;
                len -= c;
            }

            // copy data from src page to dst pages
            while (dst_iter != dst_pages.end()) {
                auto &dst_page = *dst_iter;
                auto dbegin = std::max(sbegin + delta, dst_page->offset);
                auto dend = std::min(send + delta, dst_page->offset + dst_page_size);

                std::copy(src_page->data + (dbegin - delta) - src_page->offset,
                          src_page->data + (dend - delta) - src_page->offset,
                          dst_page->data + dbegin - dst_page->offset);
                if (dend < dst_page->offset + dst_page_size)
                    break;
                ++dst_iter;
            }

            const auto c = send - sbegin;
            count -= c;
            len -= c;
            srcoff = send;
            dstoff = send + delta;
        }
        tls_pages.clear(); // drop page refs

        // zero-fill holes after the last src_page
        if (count > 0) {
            while (dst_iter != dst_pages.end()) {
                auto &dst_page = *dst_iter;
                auto dbegin = std::max(dstoff, dst_page->offset);
                auto dend = std::min(dstoff + count, dst_page->offset + dst_page_size);
                std::fill(dst_page->data + dbegin - dst_page->offset,
                          dst_page->data + dend - dst_page->offset, 0);
                ++dst_iter;
            }
            srcoff += count;
            dstoff += count;
            len -= count;
        }
        dst_pages.clear(); // drop page refs
    }

    // update object size
    if (data_len < dstoff)
        data_len = dstoff;
    return 0;
}

template<typename Functor>
int KvsStoreDataObject::truncate(uint64_t size, Functor &&page_loader) {
    data.free_pages_after(size);
    data_len = size;

    const auto page_size = data.get_page_size();
    const auto page_offset = size & ~(page_size - 1);
    if (page_offset == size)
        return 0;

    KvsPageSet::page_vector tls_pages;
    // write zeroes to the rest of the last page
    data.get_range(page_offset, page_size, tls_pages, page_loader);
    if (tls_pages.empty())
        return 0;

    auto page = tls_pages.begin();
    auto data = (*page)->data;
    std::fill(data + (size - page_offset), data + page_size, 0);
    tls_pages.clear(); // drop page ref
    return 0;
}
