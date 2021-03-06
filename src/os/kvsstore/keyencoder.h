#ifndef SRC_OS_KVSSTORE_KVIO_KEYENCODER_H_
#define SRC_OS_KVSSTORE_KVIO_KEYENCODER_H_

#include <unistd.h>
#include <stdint.h>
#include "kadi/kadi_types.h"
#include "kvsstore_debug.h"

#define KVKEY_MAX_SIZE 255

//# GROUP
//--------------------------

#define GROUP_PREFIX_ONODE    0x0
#define GROUP_PREFIX_COLL  	  0x1
#define GROUP_PREFIX_DATA  	  0x2
#define GROUP_PREFIX_JOURNAL  0x3
#define GROUP_PREFIX_OMAP     0x4
#define GROUP_PREFIX_OMAPBLK  0x5

//# encoders & decoders
//----------------------

void append_escaped(const std::string &in, std::string *out);
char *append_escaped(const std::string &in, char *key);
int decode_escaped(const char *p, std::string *out);
char *encode_nspace_oid(const std::string &nspace, const std::string &oidkey, const std::string &oidname, char *pos);
int decode_nspace_oid(char *p, ghobject_t* oid);

//# Key Structures
//------------------

struct __attribute__((__packed__)) kvs_sb_key
{
    uint32_t         prefix;                        //4B
    uint8_t          group;                         //1B
    char             name[11];                      //11B
};

struct __attribute__((__packed__)) kvs_coll_key
{
    uint32_t         prefix;						   // 4B
    uint8_t          group;						   // 1B
    // followed by cid_name
};

struct __attribute__((__packed__)) kvs_onode_key
{
    uint32_t         prefix;                       //4B
    int8_t           shardid;                      //1B 5
    uint64_t         poolid;                       //8B 13
    uint32_t         bitwisekey;                   //4B 17
    uint64_t         snapid;                       //8B 26
    uint64_t         genid;                        //8B - 34+ Bytes
    // followed by name
};

struct __attribute__((__packed__)) kvs_object_key
{
    uint8_t          group;                        //1B
    int8_t           shardid;                      //1B
    uint64_t         poolid;                       //8B
    uint32_t         bitwisekey;                   //4B
    uint64_t         snapid;                       //8B
    uint64_t         genid;                        //8B
    uint16_t         blockid;					   //2B - 32+ Bytes
    // followed by name
};

struct __attribute__((__packed__)) kvs_journal_key
{
    uint8_t          group;                        // 1B
    uint64_t         index;                        // 8B - 9 Bytes
};

struct __attribute__((__packed__)) kvs_journal_entry
{
	uint8_t         spaceid;
	uint8_t         object_type;
	uint8_t 		op_type;  						   // write or delete
	uint8_t         key_length;
	uint32_t        length;
	// followed by cid_name and bl
};

struct __attribute__((__packed__)) kvs_coll_journal_entry
{
	uint8_t         object_type;
	uint8_t 		op_type;  						   // write or delete
	uint8_t         cid_length;
	uint32_t         bl_length;
	// followed by cid_name and bl
};


// 10B
struct __attribute__((__packed__)) kvs_omap_key
{
   uint8_t       group;
   uint64_t      lid;  // unique key
   uint8_t       isheader;  // unique key
   // followed by name
   //char		     name[OMAP_KEY_MAX_SIZE];// 255-10B
};

struct __attribute__((__packed__)) kvs_omapkeyblock_key
{
    uint32_t      group;
    uint64_t      lid;      // unique key
    uint32_t      blockid;  // unique key
};




struct kvs_key_header
{
    uint8_t  hdr;
    uint8_t  isonode;
    int8_t  shardid;
    uint64_t poolid;
};




// SB Keys
// ---------------

inline uint8_t construct_kvsbkey_impl(void *buffer) {
	static const char sbkey[5] = "kvsb";
	memcpy(buffer, sbkey, 4);
	return 4;
}

// Journal Key
// -------------------
inline uint8_t construct_journalkey_impl(void *buffer, const uint64_t index) {
	kvs_journal_key* key = (kvs_journal_key*)buffer;
	key->group = GROUP_PREFIX_JOURNAL;
	key->index = index;
	return sizeof(kvs_journal_key);
}

// Collection Keys
// ---------------

inline uint8_t calculate_collkey_length(const int namelen)
{
	const int length = namelen + sizeof(kvs_coll_key);
	if (length > KVKEY_MAX_SIZE) {
		throw "Collection KEYSIZE is too long";
	}
	return length;
}

inline uint8_t construct_collkey_impl(void *buffer, const char *name, const int namelen)
{
    FTRACE

    struct kvs_coll_key *collkey = (struct kvs_coll_key *)buffer;
    collkey->prefix = GROUP_PREFIX_COLL;	// for list_collections
    collkey->group  = GROUP_PREFIX_COLL;
    //TR << "coll name = " << name << ",name len " << namelen ;
    memcpy((char*)buffer + sizeof(kvs_coll_key), name, namelen);
    //TR << "returning " <<sizeof(kvs_coll_key) + namelen ;
    return (sizeof(kvs_coll_key) + namelen);
}

// OMAP Keys
// ---------------

inline uint8_t calculate_omapkey_length(const int namelen)
{
	const int length = sizeof(kvs_omap_key) + namelen;
	if (length > KVKEY_MAX_SIZE) {
		throw "Collection KEYSIZE is too long";
	}
	return length;
}


inline uint8_t construct_omapkey_impl(void *keybuffer, uint64_t lid, const char *name, const int name_len) {
    if (sizeof(kvs_omap_key) + name_len > KVKEY_MAX_SIZE) {
        TRERR << "omap key name is too long:" << name_len << ", total key size = " << sizeof(kvs_omap_key) + name_len;
        exit(1);
    }
    struct kvs_omap_key* kvskey = (struct kvs_omap_key*)keybuffer;
    kvskey->group= GROUP_PREFIX_OMAP;
    kvskey->lid  = lid;
    kvskey->isheader = (name_len == 0);

    memcpy((char*)keybuffer + sizeof(kvs_omap_key), name, name_len);
    //TR << "OMAP KVSSD KEY " << print_kvssd_key(keybuffer, (sizeof(kvs_omap_key) + name_len));
    return (sizeof(kvs_omap_key) + name_len);
}

inline uint8_t calculate_omapblockkey_length()
{
    return sizeof(kvs_omapkeyblock_key);
}


inline uint8_t construct_omapblockkey_impl(void *keybuffer, uint64_t lid, uint32_t blockid) {

    struct kvs_omapkeyblock_key* kvskey = (struct kvs_omapkeyblock_key*)keybuffer;
    kvskey->group= GROUP_PREFIX_OMAPBLK;
    kvskey->lid  = lid;
    kvskey->blockid = blockid;
    return sizeof(kvs_omapkeyblock_key);
}

// Onode and Data Keys
// --------------------

// key -> ghobject
inline bool construct_onode_ghobject_t(CephContext* cct, char *key, int keylength, ghobject_t* oid) {
    struct kvs_onode_key* kvskey = (struct kvs_onode_key*)key;
    if (kvskey->prefix != GROUP_PREFIX_ONODE && kvskey->prefix != GROUP_PREFIX_DATA) return false;

    oid->shard_id.id = kvskey->shardid - 0x80;
    oid->hobj.pool = kvskey->poolid - 0x8000000000000000ull;
    oid->hobj.set_bitwise_key_u32(kvskey->bitwisekey);
    oid->hobj.snap.val = kvskey->snapid;
    oid->generation = kvskey->genid;

    if (decode_nspace_oid(key + sizeof(kvs_onode_key), oid) < 0) {
        ceph_abort();
    }
    return true;
}

inline bool construct_onode_ghobject_t(CephContext* cct, kv_key &key, ghobject_t* oid) {
	return construct_onode_ghobject_t(cct, (char*)key.key, key.length, oid);
}
// ghobject -> key

inline uint8_t construct_object_key(CephContext* cct, const ghobject_t& oid, void *keybuffer, uint16_t blockindex = 0) {
    struct kvs_object_key* kvskey = (struct kvs_object_key*)keybuffer;
    char *name_loc = (char*)keybuffer + sizeof(kvs_object_key);

    kvskey->group = GROUP_PREFIX_DATA;
    kvskey->shardid = int8_t(oid.shard_id) + 0x80;
    kvskey->poolid  = oid.hobj.pool + 0x8000000000000000ull;
    kvskey->bitwisekey = oid.hobj.get_bitwise_key_u32();
    kvskey->snapid = uint64_t(oid.hobj.snap);
    kvskey->genid = (uint64_t)oid.generation;
    kvskey->blockid = blockindex;

    const char *pos = encode_nspace_oid(oid.hobj.nspace, oid.hobj.get_key(), oid.hobj.oid.name, name_loc);
    const int total_keylength = (pos - name_loc) + sizeof(kvs_object_key);

    if (total_keylength > KVKEY_MAX_SIZE) {
    	std::cerr << "name is too long" << std::endl;
        ceph_abort();
    }

    //TR << "construct object key: oid=" << oid << ", keybuffer = " << print_kvssd_key(keybuffer, total_keylength) ;
    return total_keylength;
}

inline uint32_t hash_object_group_id(const uint8_t  isonode,const int8_t shardid, const uint64_t poolid) {
	struct kvs_key_header hdr = { GROUP_PREFIX_DATA, isonode, shardid, poolid };
	return ceph_str_hash_linux((char*)&hdr, sizeof(struct kvs_key_header));
}

inline uint8_t construct_onode_key(CephContext* cct, const ghobject_t& oid, void *keybuffer) {
    struct kvs_onode_key* kvskey = (struct kvs_onode_key*)keybuffer;
    char *name_loc = (char*)keybuffer + sizeof(kvs_onode_key);

    kvskey->prefix  = GROUP_PREFIX_ONODE;
    kvskey->shardid = int8_t(oid.shard_id) + 0x80;
    kvskey->poolid  = oid.hobj.pool + 0x8000000000000000ull;
    kvskey->bitwisekey = oid.hobj.get_bitwise_key_u32();
    kvskey->snapid = uint64_t(oid.hobj.snap);
    kvskey->genid = (uint64_t)oid.generation;

    //TR << "construct onode key: shard id = " << (int)int8_t(oid.shard_id) << ", encoded = " << (int)kvskey->shardid;

    const char *pos = encode_nspace_oid(oid.hobj.nspace, oid.hobj.get_key(), oid.hobj.oid.name, name_loc);
    const int total_keylength = (pos - name_loc) + sizeof(kvs_onode_key);

    if (total_keylength > KVKEY_MAX_SIZE) {
    	std::cerr << "name is too long" << std::endl;
        ceph_abort();
    }

    return total_keylength;
}

// Encoders and Decoders
// ------------------------

inline void append_escaped(const std::string &in, std::string *out)
{
    char hexbyte[8];
    for (string::const_iterator i = in.begin(); i != in.end(); ++i) {
        if (*i <= '#') {
            snprintf(hexbyte, sizeof(hexbyte), "#%02x", (uint8_t)*i);
            out->append(hexbyte);
        } else if (*i >= '~') {
            snprintf(hexbyte, sizeof(hexbyte), "~%02x", (uint8_t)*i);
            out->append(hexbyte);
        } else {
            out->push_back(*i);
        }
    }
    out->push_back('!');
}


inline char *append_escaped(const std::string &in, char *key)
{
    char *pos = key;
    for (string::const_iterator i = in.begin(); i != in.end(); ++i) {
        if (*i <= '#') {
            pos += snprintf(pos, 8, "#%02x", (uint8_t)*i);

        } else if (*i >= '~') {
            pos += snprintf(pos, 8, "~%02x", (uint8_t)*i);
        } else {
            *pos = *i;
            pos += 1;
        }
    }
    *pos = '!';
    pos += 1;
    return pos;
}

inline int decode_escaped(const char *p, std::string *out)
{
    const char *orig_p = p;
    while (*p && *p != '!') {
        if (*p == '#' || *p == '~') {
            unsigned hex;
            int r = sscanf(++p, "%2x", &hex);
            if (r < 1)
                return -EINVAL;
            out->push_back((char)hex);
            p += 2;
        } else {
            out->push_back(*p++);
        }
    }
    return p - orig_p;
}



inline char *encode_nspace_oid(const std::string &nspace, const std::string &oidkey, const std::string &oidname, char *pos)
{
    pos = append_escaped(nspace, pos);

    if (oidkey.length()) {
        // is a key... could be < = or >.
        // (ASCII chars < = and > sort in that order, yay)
        if (oidkey < oidname) {
            *pos++ = '<';
            pos = append_escaped(oidkey, pos);
            pos = append_escaped(oidname, pos);
        } else if (oidkey > oidname) {
            *pos++ = '>';
            pos = append_escaped(oidkey, pos);
            pos = append_escaped(oidname, pos);
        } else {
            // same as no key
            *pos++ = '=';
            pos = append_escaped(oidname, pos);
        }
    } else {
        // no key
        *pos++ = '=';
        pos = append_escaped(oidname, pos);
    }

    return pos;
}

inline int decode_nspace_oid(char *p, ghobject_t* oid)
{
    int r;
    p += decode_escaped(p, &oid->hobj.nspace) + 1;

    if (*p == '=') {
        // no key
        ++p;
        r = decode_escaped(p, &oid->hobj.oid.name);
        if (r < 0)
            return -7;
        p += r + 1;
    } else if (*p == '<' || *p == '>') {
        // key + name
        ++p;
        string okey;
        r = decode_escaped(p, &okey);
        if (r < 0)
            return -8;
        p += r + 1;
        r = decode_escaped(p, &oid->hobj.oid.name);
        if (r < 0)
            return -9;
        p += r + 1;
        oid->hobj.set_key(okey);
    } else {
        // malformed
        return -10;
    }

    return 0;

}


#endif /* SRC_OS_KVSSTORE_KVIO_KEYENCODER_H_ */
