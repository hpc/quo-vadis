/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2020-2026 Triad National Security, LLC
 *                         All rights reserved.
 *
 * Copyright (c) 2020-2021 Lawrence Livermore National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-hwloc.h
 */

#ifndef QVI_HWLOC_H
#define QVI_HWLOC_H

#include "qvi-common.h"

// Forward declarations.
struct qvi_hwloc_bitmap;
struct qvi_hwloc_device;

/** Set of device identifiers. */
using qvi_hwloc_dev_id_set = std::unordered_set<std::string>;
/** Device list type. */
using qvi_hwloc_dev_list = std::vector<
    std::shared_ptr<qvi_hwloc_device>
>;

/** Maps supported device types to a list of devices of that type. */
using qvi_hwloc_dev_map = std::map<
    qv_hw_obj_type_t, qvi_hwloc_dev_list
>;

/** Flags that influence how the hwloc instance behaves. */
typedef long long qvi_hwloc_flags_t;
/** Empty hwloc flags. */
const qvi_hwloc_flags_t QVI_HWLOC_FLAG_EMPTY = (0LL);
/** Provides a full system topology. */
const qvi_hwloc_flags_t QVI_HWLOC_FLAG_TOPO_FULL = (1LL<<0);
/** Disables use of SMT from the full system topology. */
const qvi_hwloc_flags_t QVI_HWLOC_FLAG_TOPO_NO_SMT = (1LL<<1);
/** Indicates that the topology was loaded from exported XML file. */
const qvi_hwloc_flags_t QVI_HWLOC_FLAG_TOPO_XML = (1LL<<2);

const qvi_hwloc_flags_t QVI_HWLOC_TOPO_MASK = 0x0000000000000003LL;

/** Internal resource type identifiers. */
enum qvi_hwloc_res_class {
    QVI_HWLOC_RES_CLASS_HOST = 0,
    QVI_HWLOC_RES_CLASS_DEV,
    QVI_HWLOC_RES_CLASS_LAST
};

struct qvi_hwloc {
private:
    enum task_xop_obj_id {
        TASK_INTERSECTS_OBJ = 0,
        TASK_INCLUDEDIN_OBJ
    };
    /** My flags. */
    qvi_hwloc_flags_t m_flags = QVI_HWLOC_FLAG_EMPTY;
    /** The cached node topology. */
    hwloc_topology_t m_topo = nullptr;
    /** Path to exported hardware topology. */
    std::string m_topo_file;
    /** Map of device types to lists of devices of those types. */
    qvi_hwloc_dev_map m_devmap;
    /** */
    int
    m_topo_set_from_xml(
        const std::string &path
    );
    /** */
    static int
    s_topo_fopen(
        const std::string &path,
        int *fd
    );
    /**
     *
     */
    int
    m_set_device_affinity_by_pci_bus_id(
        const std::string &busid,
        qvi_hwloc_device *dev
    );
    /** */
    int
    m_discover_devices(void);
    /** */
    int
    m_set_device_info(
        hwloc_obj_t obj,
        const std::string &pci_bus_id,
        qvi_hwloc_device *device
    );
    /** */
    int
    m_task_obj_xop_by_type_id(
        qv_hw_obj_type_t type,
        pid_t task_id,
        int type_index,
        task_xop_obj_id opid,
        int *result
    );
    /** */
    int
    m_obj_get_by_type(
        qv_hw_obj_type_t type,
        int type_index,
        hwloc_obj_t *obj
    );
    /** */
    int
    m_get_logical_bind_string(
        hwloc_const_bitmap_t bitmap,
        std::string &result
    ) const;
    /** */
    int
    m_get_physical_bind_string(
        hwloc_const_bitmap_t bitmap,
        std::string &result
    ) const;
    /** */
    int
    m_get_nobjs_in_cpuset(
        qv_hw_obj_type_t target_obj,
        hwloc_const_cpuset_t cpuset,
        size_t &nobjs
    ) const;
    /** */
    int
    m_get_nosdevs_in_cpuset(
        const qvi_hwloc_dev_list &devs,
        hwloc_const_cpuset_t cpuset,
        size_t &nobjs
    ) const;

    int
    m_split_cpuset_by_range(
        const qvi_hwloc_bitmap &bitmap,
        uint_t base,
        uint_t extent,
        qvi_hwloc_bitmap &result
    ) const;
    /**
     * Restricts the hardware topology such that SMT is disabled.
     */
    int
    m_disable_smt(void);
public:
    /** */
    static
    std::vector<qvi_hwloc_flags_t>
    topo_types(void) {
        static const std::vector<qvi_hwloc_flags_t> topo_type_flags = {
            QVI_HWLOC_FLAG_TOPO_FULL,
            QVI_HWLOC_FLAG_TOPO_NO_SMT
        };
        return topo_type_flags;
    }
    /** */
    static int
    bitmap_calloc(
        hwloc_cpuset_t *cpuset
    );
    /** */
    static void
    bitmap_delete(
        hwloc_cpuset_t *cpuset
    );
    /** */
    static int
    bitmap_copy(
        hwloc_const_cpuset_t src,
        hwloc_cpuset_t dest
    );
    /** */
    static int
    bitmap_dup(
        hwloc_const_cpuset_t src,
        hwloc_cpuset_t *dest
    );
    /**
     * Returns the number of bits required to represent a given cpuset.
     */
    static int
    bitmap_nbits(
        hwloc_const_cpuset_t cpuset,
        size_t *nbits
    );
    /**
     * Like hwloc_bitmap_asprintf(), but returns a string.
     */
    static std::string
    bitmap_string(
        const qvi_hwloc_bitmap &bitmap
    );
    /**
     * Like hwloc_bitmap_list_asprintf(), but returns a string.
     */
    static std::string
    bitmap_list_string(
        hwloc_const_cpuset_t cpuset
    );
    /**
     *
     */
    static int
    bitmap_sscanf(
        hwloc_cpuset_t cpuset,
        char *str
    );
    /**
     *
     */
    int
    bitmap_split(
        const qvi_hwloc_bitmap &bitmap,
        size_t npieces,
        std::vector<qvi_hwloc_bitmap> &result
    ) const;
    /** Constructor */
    qvi_hwloc(void) = default;
    /** Destructor */
    ~qvi_hwloc(void);
    /** Delete assignment operator. */
    void
    operator=(const qvi_hwloc &) = delete;
    /** Delete copy constructor. */
    qvi_hwloc(const qvi_hwloc &) = delete;
    /** Returns our flags. */
    qvi_hwloc_flags_t
    flags(void)
    const {
        return m_flags;
    }
    /**
     * Initialize hardware topology. If an XML path is provided, set
     * the hardware topology based on the contents of the XML file.
     */
    int
    topology_init(
        qvi_hwloc_flags_t flags,
        const std::string &xml_path = ""
    );
    /**
     *
     */
    int
    topology_load(void);
    /**
     *
     */
    int
    topology_export(
        const std::string &base_path
    );
    /**
     *
     */
    hwloc_topology_t
    topology_get(void);
    /**
     * Returns the path to the exported topology XML file.
     */
    std::string
    topology_file(void);
    /**
     *
     */
    bool
    topology_is_this_system(void);
    /**
     * Returns a cpuset that obeys cgroups. That is, return a cpuset
     * that is allowed when under restrictions by other mechanisms.
     */
    hwloc_const_cpuset_t
    topology_get_cpuset(void);
    /**
     * Returns a cpuset that includes all topological information provided by
     * hwloc, including that which is disallowed or restricted by other
     * mechanisms such as cgroups.
     */
    hwloc_const_cpuset_t
    topology_get_disallowed_cpuset(void);
    /**
     * Returns the underlying hwloc type from the given qv_hw_obj_type_t type.
     */
    static hwloc_obj_type_t
    obj_get_type(
        qv_hw_obj_type_t external
    );
    /**
     * Returns the type's general resource class.
     */
    static qvi_hwloc_res_class
    obj_res_class(
        qv_hw_obj_type_t type
    );
    /**
     *
     */
    int
    obj_type_depth(
        qv_hw_obj_type_t type,
        int *depth
    ) const;
    /**
     *
     */
    int
    get_devices_included_in_cpuset(
        qv_hw_obj_type_t obj_type,
        hwloc_const_cpuset_t cpuset,
        qvi_hwloc_dev_list &devs
    ) const;
    /**
     *
     */
    int
    get_device_id_in_cpuset(
        qv_hw_obj_type_t dev_obj,
        int i,
        hwloc_const_cpuset_t cpuset,
        qv_device_id_type_t dev_id_type,
        std::string &dev_id
    );
    /**
     *
     */
    int
    devices_emit(
        qv_hw_obj_type_t obj_type
    ) const;
    /**
     * Returns a reference to vector of supported device types.
     */
    static const std::vector<qv_hw_obj_type_t> &
    supported_devices(void);
    /**
     *
     */
    int
    task_get_cpubind(
        pid_t who,
        qvi_hwloc_bitmap &result
    );
    /**
     *
     */
    int
    task_set_cpubind_from_cpuset(
        pid_t task_id,
        hwloc_const_cpuset_t cpuset
    );
    /** */
    int
    task_intersects_obj_by_type_id(
        qv_hw_obj_type_t type,
        pid_t task_id,
        int type_index,
        int *result
    );
    /**
     *
     */
    int
    task_includedin_obj_by_type_id(
        qv_hw_obj_type_t type,
        pid_t task_id,
        int type_index,
        int *result
    );
    int
    bind_string(
        hwloc_const_bitmap_t bitmap,
        qv_bind_string_flags_t flags,
        char **result
    );
    /** */
    int
    get_nobjs_by_type(
       qv_hw_obj_type_t target_type,
       int *out_nobjs
    );
    /** */
    int
    get_nobjs_in_cpuset(
        qv_hw_obj_type_t target_obj,
        hwloc_const_cpuset_t cpuset,
        size_t &nobjs
    ) const;
    /** */
    int
    get_obj_in_cpuset_by_depth(
        hwloc_const_cpuset_t cpuset,
        int depth,
        int index,
        hwloc_obj_t *result_obj
    ) const;
    /** */
    int
    get_cpuset_for_nobjs(
        const qvi_hwloc_bitmap &cpuset,
        qv_hw_obj_type_t obj_type,
        uint_t nobjs,
        qvi_hwloc_bitmap &result
    );
};

/**
 * Maintains multiple hwloc instances that are required to support provided
 * functionality. In general, we want to maintain a small number here to keep
 * our memory requirements as low as possible.
 */
struct qvi_hwlocs {
private:
    /** Maps hardware topology type IDs to the appropriate hwloc instance. */
    using qvi_hwloc_type_map = std::map<qvi_hwloc_flags_t, qvi_hwloc *>;
    /** Maps a topology type to an hwloc instance. */
    qvi_hwloc_type_map m_type2hwloc;
public:
    /** Constructor. */
    qvi_hwlocs(void) {
        // Initialize the table of hardware topologies we support.
        for (const auto topo_type : qvi_hwloc::topo_types()) {
            m_type2hwloc.emplace(topo_type, new qvi_hwloc());
        }
    }
    /** Destructor. */
    ~qvi_hwlocs(void)
    {
        for (auto i : m_type2hwloc) {
            delete i.second;
        }
    }
    /** Delete assignment operator. */
    void
    operator=(const qvi_hwlocs &) = delete;
    /** Delete copy constructor. */
    qvi_hwlocs(const qvi_hwlocs &) = delete;
    /** Returns the appropriate instance given the provided flags. */
    qvi_hwloc &
    get(
        qvi_hwloc_flags_t flags
    ) {
        const auto got = m_type2hwloc.find(flags & QVI_HWLOC_TOPO_MASK);
        if (qvi_unlikely(got == m_type2hwloc.end())) {
            // If this happens, this is a bug.
            qvi_abort();
        }
        return *got->second;
    }
};

/**
 * hwloc bitmap object.
 */
struct qvi_hwloc_bitmap {
    friend class cereal::access;
private:
    /** Internal bitmap. */
    hwloc_bitmap_t m_data = nullptr;
public:
    /** Default constructor. */
    qvi_hwloc_bitmap(void)
    {
        const int rc = qvi_hwloc::bitmap_calloc(&m_data);
        if (qvi_unlikely(rc != QV_SUCCESS)) throw qvi_runtime_error(rc);
    }
    /** Construct via hwloc_const_bitmap_t. */
    explicit qvi_hwloc_bitmap(hwloc_const_bitmap_t bitmap)
    {
        int rc = qvi_hwloc::bitmap_calloc(&m_data);
        if (qvi_unlikely(rc != QV_SUCCESS)) throw qvi_runtime_error(rc);
        rc = set(bitmap);
        if (qvi_unlikely(rc != QV_SUCCESS)) throw qvi_runtime_error(rc);
    }
    /** Copy constructor. */
    qvi_hwloc_bitmap(const qvi_hwloc_bitmap &src)
    {
        int rc = qvi_hwloc::bitmap_calloc(&m_data);
        if (qvi_unlikely(rc != QV_SUCCESS)) throw qvi_runtime_error(rc);
        rc = set(src.m_data);
        if (qvi_unlikely(rc != QV_SUCCESS)) throw qvi_runtime_error(rc);
    }
    /** Destructor. */
    ~qvi_hwloc_bitmap(void)
    {
        qvi_hwloc::bitmap_delete(&m_data);
    }
    /** Equality operator. */
    bool
    operator==(
        const qvi_hwloc_bitmap &x
    ) const {
        return hwloc_bitmap_compare(cdata(), x.cdata()) == 0;
    }
    /** Assignment operator. */
    void
    operator=(const qvi_hwloc_bitmap &src)
    {
        const int rc = qvi_hwloc::bitmap_copy(src.m_data, m_data);
        if (qvi_unlikely(rc != QV_SUCCESS)) throw qvi_runtime_error(rc);
    }
    /** Sets the object's internal bitmap to match src's. */
    int
    set(hwloc_const_bitmap_t src)
    {
        return qvi_hwloc::bitmap_copy(src, m_data);
    }
    /**
     * Returns a hwloc_bitmap_t that allows
     * modification of internal instance data.
     */
    hwloc_bitmap_t
    data(void)
    {
        return m_data;
    }
    /**
     * Returns a hwloc_const_bitmap_t that
     * cannot modify internal instance data.
     */
    hwloc_const_bitmap_t
    cdata(void) const
    {
        return m_data;
    }
    /**
     * Or (|) operator overload.
     */
    qvi_hwloc_bitmap
    operator|(
        const qvi_hwloc_bitmap &rhs
    ) {
        qvi_hwloc_bitmap result;
        const int orrc = hwloc_bitmap_or(
            result.data(), cdata(), rhs.cdata()
        );
        if (qvi_unlikely(orrc != 0)) throw qvi_runtime_error(QV_ERR_HWLOC);
        return result;
    }
    /**
     * Returns the result of an hwloc_bitmap_or over the provided bitmaps.
     */
    static qvi_hwloc_bitmap
    op_or(
        const std::vector<qvi_hwloc_bitmap> &bitmaps
    ) {
        qvi_hwloc_bitmap result;
        for (const auto &bitmap : bitmaps) {
            result = result | bitmap;
        }
        return result;
    }
    /**
     * Serializes a qvi_hwloc_bitmap.
     */
    template<class Archive>
    void
    save(
        Archive &archive
    ) const {
        // We are sending the string representation of the cpuset.
        archive(qvi_hwloc::bitmap_string(qvi_hwloc_bitmap(m_data)));
    }
    /**
     * Deserializes a qvi_hwloc_bitmap.
     */
    template<class Archive>
    void
    load(
        Archive &archive
    ) {
        std::string bitmaps;
        archive(bitmaps);
        const int rc = qvi_hwloc::bitmap_sscanf(
            m_data, const_cast<char *>(bitmaps.c_str())
        );
        if (qvi_unlikely(rc != QV_SUCCESS)) throw qvi_runtime_error(rc);
    }
};

struct qvi_hwloc_device {
    /** ID used for invisible devices. */
    static constexpr int INVISIBLE_ID = -1;
    /** ID used to indicate an invalid or unset ID. */
    static constexpr int INVALID_ID = -1;
    /** Device type. */
    qv_hw_obj_type_t type = QV_HW_OBJ_LAST;
    /** Device affinity. */
    qvi_hwloc_bitmap affinity;
    /** Device ID. Note: this is not the device's ordinal. */
    int id = INVISIBLE_ID;
    /** Device name. */
    std::string name = {};
    /** PCI bus ID. */
    std::string pci_bus_id = {};
    /** Universally Unique Identifier. */
    std::string uuid = {};
};

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
