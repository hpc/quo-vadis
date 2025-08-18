/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2020-2025 Triad National Security, LLC
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

/** Vector of bitmap objects. */
using qvi_hwloc_bitmaps = std::vector<qvi_hwloc_bitmap>;
/** Set of device identifiers. */
using qvi_hwloc_dev_id_set = std::unordered_set<std::string>;
/** Device list type. */
using qvi_hwloc_dev_list = std::vector<
    std::shared_ptr<qvi_hwloc_device>
>;

struct qvi_hwloc {
private:
    enum task_xop_obj_id {
        TASK_INTERSECTS_OBJ = 0,
        TASK_INCLUDEDIN_OBJ
    };
    /** The cached node topology. */
    hwloc_topology_t m_topo = nullptr;
    /** Path to exported hardware topology. */
    std::string m_topo_file;
    /** Cached set of PCI IDs discovered during topology load. */
    qvi_hwloc_dev_id_set m_device_ids;
    /** Cached devices discovered during topology load. */
    qvi_hwloc_dev_list m_devices;
    /** Cached GPUs discovered during topology load. */
    qvi_hwloc_dev_list m_gpus;
    /** Cached NICs discovered during topology load. */
    qvi_hwloc_dev_list m_nics;
    /** */
    int
    m_topo_set_from_xml(
        const std::string &path
    );
    /** */
    static int
    s_topo_fopen(
        const char *path,
        int *fd
    );
    /**
     * First pass: discover devices that must be added to the list of devices.
     */
    int
    m_discover_devices_first_pass(void);
    /** */
    int
    m_discover_nic_devices(void);
    /** */
    int
    m_discover_gpu_devices(void);
    /** */
    int
    m_discover_devices(void);
    /** */
    int
    m_set_general_device_info(
        hwloc_obj_t obj,
        hwloc_obj_t pci_obj,
        const std::string &pci_bus_id,
        qvi_hwloc_device *device
    );
    /** */
    int
    m_set_gpu_device_info(
        hwloc_obj_t obj,
        qvi_hwloc_device *device
    );
    /** */
    int
    m_set_of_device_info(
        hwloc_obj_t obj,
        qvi_hwloc_device *device
    );
    /** */
    int
    m_get_proc_cpubind(
        pid_t who,
        qvi_hwloc_bitmap &result
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
    );
    /** */
    int
    m_get_physical_bind_string(
        hwloc_const_bitmap_t bitmap,
        std::string &result
    );
    /** */
    int
    m_get_nobjs_in_cpuset(
        qv_hw_obj_type_t target_obj,
        hwloc_const_cpuset_t cpuset,
        int *nobjs
    );
    /** */
    int
    m_get_nosdevs_in_cpuset(
        const qvi_hwloc_dev_list &devs,
        hwloc_const_cpuset_t cpuset,
        int *nobjs
    );

    int
    m_split_cpuset_chunk_size(
        hwloc_const_cpuset_t cpuset,
        uint_t npieces,
        uint_t *chunk_size
    );

    int
    m_split_cpuset_by_range(
        hwloc_const_cpuset_t cpuset,
        uint_t base,
        uint_t extent,
        hwloc_bitmap_t result
    );
public:
    /** */
    static void
    bitmap_debug(
        cstr_t msg,
        hwloc_const_cpuset_t cpuset
    );
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
        hwloc_const_cpuset_t cpuset
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
    bitmap_split_by_chunk_id(
        hwloc_const_cpuset_t bitmap,
        uint_t nchunks,
        uint_t chunk_id,
        hwloc_cpuset_t result
    );
    /** Constructor */
    qvi_hwloc(void) = default;
    /** Destructor */
    ~qvi_hwloc(void);
    /** Delete assignment operator. */
    void
    operator=(const qvi_hwloc &) = delete;
    /** Delete copy constructor. */
    qvi_hwloc(const qvi_hwloc &) = delete;
    /**
     * Initialize hardware topology. If an XML path is provided, set
     * the hardware topology based on the contents of the XML file.
     */
    int
    topology_init(
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
        const std::string &base_path,
        std::string &path
    );
    /**
     *
     */
    hwloc_topology_t
    topology_get(void);
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
     * Returns whether the provided type is a host resource (e.g., core, NUMA node).
     */
    static bool
    obj_is_host_resource(
        qv_hw_obj_type_t type
    );
    /**
     *
     */
    int
    obj_type_depth(
        qv_hw_obj_type_t type,
        int *depth
    );
    /**
     *
     */
    int
    get_devices_in_cpuset(
        qv_hw_obj_type_t obj_type,
        hwloc_const_cpuset_t cpuset,
        qvi_hwloc_dev_list &devs
    );
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
    );
    /**
     *
     */
    int
    get_device_affinity(
        qv_hw_obj_type_t dev_obj,
        int device_id,
        qvi_hwloc_bitmap &cpuset
    );
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
        int *nobjs
    );
    /** */
    int
    get_obj_in_cpuset_by_depth(
        hwloc_const_cpuset_t cpuset,
        int depth,
        int index,
        hwloc_obj_t *result_obj
    );
    /**
     *
     */
    int
    get_obj_type_in_cpuset(
        int npieces,
        hwloc_const_cpuset_t cpuset,
        qv_hw_obj_type_t *target_obj
    );
    /** */
    int
    get_cpuset_for_nobjs(
        hwloc_const_cpuset_t cpuset,
        qv_hw_obj_type_t obj_type,
        uint_t nobjs,
        qvi_hwloc_bitmap &result
    );
    /**
     * Takes a bitmap and returns a new one with SMT disabled.
     */
    qvi_hwloc_bitmap
    bitmap_disable_smt(
        const qvi_hwloc_bitmap &bitmap
    );
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
     * Returns the result of an hwloc_bitmap_or over the provided bitmaps.
     */
    static qvi_hwloc_bitmap
    op_or(
        const qvi_hwloc_bitmaps &bitmaps
    ) {
        qvi_hwloc_bitmap result;
        for (const auto &bitmap : bitmaps) {
            const int orrc = hwloc_bitmap_or(
                result.data(), result.cdata(), bitmap.cdata()
            );
            if (qvi_unlikely(orrc != 0)) throw qvi_runtime_error(QV_ERR_HWLOC);
        }
        return result;
    }

    template<class Archive>
    void
    save(
        Archive &archive
    ) const {
        // We are sending the string representation of the cpuset.
        archive(qvi_hwloc::bitmap_string(m_data));
    }

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
    /** Vendor ID. */
    int vendor_id = INVALID_ID;
    /** System Management Interface ID. */
    int smi = INVALID_ID;
    /** CUDA/ROCm visible devices ID. */
    int id = INVISIBLE_ID;
    /** Device name. */
    std::string name;
    /** PCI bus ID. */
    std::string pci_bus_id;
    /** Universally Unique Identifier. */
    std::string uuid;
};

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
