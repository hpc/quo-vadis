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
 * @file qvi-hwloc.cc
 */

#include "qvi-hwloc.h"
#include "qvi-utils.h"

#include "qvi-nvml.h"
#include "qvi-rsmi.h"

/** Maps a string identifier to a device. */
using qvi_hwloc_dev_map = std::unordered_map<
    std::string,
    std::shared_ptr<qvi_hwloc_device>
>;

static bool
dev_list_compare_by_visdev_id(
    const std::shared_ptr<qvi_hwloc_device> &a,
    const std::shared_ptr<qvi_hwloc_device> &b
) {
    return a.get()->id < b.get()->id;
}

/**
 * Converts the provided hwloc object type to its corresponding internal type.
 */
static qv_hw_obj_type_t
obj_hwloc2qv(
   hwloc_obj_type_t external
) {
    switch(external) {
        case(HWLOC_OBJ_MACHINE):
             return QV_HW_OBJ_MACHINE;
        case(HWLOC_OBJ_PACKAGE):
             return QV_HW_OBJ_PACKAGE;
        case(HWLOC_OBJ_CORE):
             return QV_HW_OBJ_CORE;
        case(HWLOC_OBJ_PU):
             return QV_HW_OBJ_PU;
        case(HWLOC_OBJ_L1CACHE):
             return QV_HW_OBJ_L1CACHE;
        case(HWLOC_OBJ_L2CACHE):
             return QV_HW_OBJ_L2CACHE;
        case(HWLOC_OBJ_L3CACHE):
             return QV_HW_OBJ_L3CACHE;
        case(HWLOC_OBJ_L4CACHE):
             return QV_HW_OBJ_L4CACHE;
        case(HWLOC_OBJ_L5CACHE):
             return QV_HW_OBJ_L5CACHE;
        case(HWLOC_OBJ_NUMANODE):
            return QV_HW_OBJ_NUMANODE;
        case(HWLOC_OBJ_OS_DEVICE):
            return QV_HW_OBJ_GPU;
        default:
            // This is an internal development error.
            qvi_abort();
    }
}

/**
 * Returns a pointer to the associated PCI object, nullptr otherwise.
 */
static hwloc_obj_t
get_pci_obj(
    hwloc_obj_t dev
) {
    if (dev->type == HWLOC_OBJ_PCI_DEVICE) return dev;
    if (dev->parent->type == HWLOC_OBJ_PCI_DEVICE) return dev->parent;
    return nullptr;
}

/**
 * Get the PCI bus ID of an input device; save it into input string.
 * Returns a pointer to the associated PCI object, nullptr otherwise.
 */
static hwloc_obj_t
get_pci_busid(
    hwloc_obj_t dev,
    std::string &busid
) {
    hwloc_obj_t pcidev = get_pci_obj(dev);
    if (!pcidev) return nullptr;

    static constexpr int PCI_BUS_ID_BUFF_SIZE = 32;
    char busid_buffer[PCI_BUS_ID_BUFF_SIZE] = {'\0'};
    const int np = snprintf(
        busid_buffer, PCI_BUS_ID_BUFF_SIZE,
        "%04x:%02x:%02x.%01x",
        pcidev->attr->pcidev.domain,
        pcidev->attr->pcidev.bus,
        pcidev->attr->pcidev.dev,
        pcidev->attr->pcidev.func
    );
    if (qvi_unlikely(np >= PCI_BUS_ID_BUFF_SIZE)) {
        throw qvi_runtime_error(QV_ERR_INTERNAL);
    }
    busid = std::string(busid_buffer);
    return pcidev;
}

static int
set_visdev_id(
    qvi_hwloc_device *device
) {
    const qv_hw_obj_type_t type = device->type;
    // Consider only what is listed here.
    if (type != QV_HW_OBJ_GPU) {
        return QV_SUCCESS;
    }
    // Set visible devices.  Note that these IDs are relative to a
    // particular context, so we need to keep track of that. For example,
    // visdevs=2,3 could be 0,1.  The challenge is in supporting a user's
    // request via (e.g, CUDA_VISIBLE_DEVICES, ROCR_VISIBLE_DEVICES).
    int id = qvi_hwloc_device::INVISIBLE_ID, id2 = id;
    if (sscanf(device->name.c_str(), "cuda%d", &id) == 1) {
        device->id = id;
        return QV_SUCCESS;
    }
    if (sscanf(device->name.c_str(), "rsmi%d", &id) == 1) {
        device->id = id;
        return QV_SUCCESS;
    }
    if (sscanf(device->name.c_str(), "opencl%dd%d", &id2, &id) == 2) {
        device->id = id;
        return QV_SUCCESS;
    }
    return QV_SUCCESS;
}

static std::string
topo_fname(
    const std::string &base
) {
    return base + "/hwtopo." + std::to_string(getpid()) + ".xml";
}

void
qvi_hwloc::bitmap_debug(
    cstr_t msg,
    hwloc_const_cpuset_t cpuset
) {
#if QVI_DEBUG_MODE == 0
    qvi_unused(msg);
#endif
    if (!cpuset) throw qvi_runtime_error(QV_ERR_INTERNAL);
    qvi_log_debug("{} BITMAP={}", msg, qvi_hwloc::bitmap_string(cpuset));
}

int
qvi_hwloc::bitmap_calloc(
    hwloc_cpuset_t *cpuset
) {
    *cpuset = hwloc_bitmap_alloc();
    if (qvi_unlikely(!*cpuset)) return QV_ERR_OOR;
    hwloc_bitmap_zero(*cpuset);
    return QV_SUCCESS;
}

void
qvi_hwloc::bitmap_delete(
    hwloc_cpuset_t *cpuset
) {
    if (qvi_unlikely(!cpuset)) return;
    if (*cpuset) hwloc_bitmap_free(*cpuset);
    *cpuset = nullptr;
}

int
qvi_hwloc::bitmap_copy(
    hwloc_const_cpuset_t src,
    hwloc_cpuset_t dest
) {
    assert(src && dest);
    const int rc = hwloc_bitmap_copy(dest, src);
    if (qvi_unlikely(rc != 0)) {
        return QV_ERR_HWLOC;
    }
    return QV_SUCCESS;
}

int
qvi_hwloc::bitmap_dup(
    hwloc_const_cpuset_t src,
    hwloc_cpuset_t *dest
) {
    int rc = QV_SUCCESS;
    hwloc_cpuset_t idest = nullptr;
    do {
        rc = qvi_hwloc::bitmap_calloc(&idest);
        if (qvi_unlikely(rc != QV_SUCCESS)) break;

        rc = qvi_hwloc::bitmap_copy(src, idest);
    } while (false);

    if (qvi_unlikely(rc != QV_SUCCESS)) {
        qvi_hwloc::bitmap_delete(&idest);
    }
    *dest = idest;
    return rc;
}

int
qvi_hwloc_bitmap_nbits(
    hwloc_const_cpuset_t cpuset,
    size_t *nbits
) {
    *nbits = 0;

    const int inbits = hwloc_bitmap_last(cpuset);
    if (qvi_unlikely(inbits == -1)) return QV_ERR_HWLOC;

    *nbits = size_t(inbits) + 1;
    return QV_SUCCESS;
}

std::string
qvi_hwloc::bitmap_string(
    hwloc_const_cpuset_t cpuset
) {
    char *iresult = nullptr;
    (void)hwloc_bitmap_asprintf(&iresult, cpuset);
    if (qvi_unlikely(!iresult)) throw qvi_runtime_error(QV_ERR_OOR);

    std::string result(iresult);
    free(iresult);
    return result;
}

std::string
qvi_hwloc::bitmap_list_string(
    hwloc_const_cpuset_t cpuset
) {
    char *iresult = nullptr;
    (void)hwloc_bitmap_list_asprintf(&iresult, cpuset);
    if (qvi_unlikely(!iresult)) throw qvi_runtime_error(QV_ERR_OOR);

    std::string result(iresult);
    free(iresult);
    return result;
}

int
qvi_hwloc::bitmap_sscanf(
    hwloc_cpuset_t cpuset,
    char *str
) {
    const int rc = hwloc_bitmap_sscanf(cpuset, str);
    if (qvi_unlikely(rc != 0)) {
        return QV_ERR_HWLOC;
    }
    return QV_SUCCESS;
}

int
qvi_hwloc::bitmap_split_by_chunk_id(
    hwloc_const_cpuset_t bitmap,
    uint_t nchunks,
    uint_t chunk_id,
    hwloc_cpuset_t result
) {
    uint_t chunk_size = 0;
    const int rc = m_split_cpuset_chunk_size(
        bitmap, nchunks, &chunk_size
    );
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;
    // 0 chunk_size likely caused by nonsensical split request.
    // Chunk IDs must be < nchunks: 0, 1, ... , nchunks-1.
    if (chunk_size == 0 || chunk_id >= nchunks) {
        return QV_ERR_SPLIT;
    }

    return m_split_cpuset_by_range(
        bitmap, chunk_size * chunk_id, chunk_size, result
    );
}

qvi_hwloc::~qvi_hwloc(void)
{
    if (m_topo) hwloc_topology_destroy(m_topo);
}

int
qvi_hwloc::m_topo_set_from_xml(
    const std::string &path
) {
    const int rc = hwloc_topology_set_xml(m_topo, path.c_str());
    if (qvi_unlikely(rc == -1)) {
        qvi_log_error("hwloc_topology_set_xml() failed");
        return QV_ERR_HWLOC;
    }
    return QV_SUCCESS;
}

int
qvi_hwloc::topology_init(
    const std::string &xml_path
) {
    const int rc = hwloc_topology_init(&m_topo);
    if (qvi_unlikely(rc != 0)) {
        qvi_log_error("hwloc_topology_init() failed");
        return QV_ERR_HWLOC;
    }
    if (!xml_path.empty()) return m_topo_set_from_xml(xml_path);
    return QV_SUCCESS;
}

int
qvi_hwloc::topology_load(void)
{
    int rc = QV_SUCCESS;
    cstr_t ers = nullptr;
    do {
        // Set flags that influence hwloc's behavior. Include resources that are
        // not allowed (e.g., by cgroups) in the base topology. We will have
        // functions that provide bitmap access to allowed and disallowed
        // resources depending on need, but we must load it all.
        const uint_t flags = HWLOC_TOPOLOGY_FLAG_INCLUDE_DISALLOWED;
        rc = hwloc_topology_set_flags(m_topo, flags);
        if (qvi_unlikely(rc != 0)) {
            ers = "hwloc_topology_set_flags() failed";
            rc = QV_ERR_HWLOC;
            break;
        }

        rc = hwloc_topology_set_all_types_filter(
            m_topo, HWLOC_TYPE_FILTER_KEEP_IMPORTANT
        );
        if (qvi_unlikely(rc != 0)) {
            ers = "hwloc_topology_set_all_types_filter() failed";
            rc = QV_ERR_HWLOC;
            break;
        }

        rc = hwloc_topology_set_type_filter(
            m_topo,
            HWLOC_OBJ_OS_DEVICE,
            HWLOC_TYPE_FILTER_KEEP_IMPORTANT
        );
        if (qvi_unlikely(rc != 0)) {
            ers = "hwloc_topology_set_type_filter() failed";
            rc = QV_ERR_HWLOC;
            break;
        }

        rc = hwloc_topology_load(m_topo);
        if (qvi_unlikely(rc != 0)) {
            ers = "hwloc_topology_load() failed";
            rc = QV_ERR_HWLOC;
            break;
        }

        rc = m_discover_devices();
        if (qvi_unlikely(rc != QV_SUCCESS)) {
            ers = "m_discover_devices() failed";
        }
    } while (false);

    if (qvi_unlikely(ers)) {
        qvi_log_error("{} with rc={} ({})", ers, rc, qv_strerr(rc));
    }
    return rc;
}

/**
 *
 */
int
qvi_hwloc::s_topo_fopen(
    const char *path,
    int *fd
) {
    const int ifd = open(path, O_CREAT | O_RDWR, 0644);
    if (qvi_unlikely(ifd == -1)) {
        const int err = errno;
        cstr_t ers = "open() failed";
        qvi_log_error("{} {}", ers, strerror(err));
        return QV_ERR_FILE_IO;
    }
    // We need to publish this file to consumers that are potentially not part
    // of our group. We cannot assume the current umask, so set explicitly.
    const int rc = fchmod(ifd, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (qvi_unlikely(rc == -1)) {
        const int err = errno;
        cstr_t ers = "fchmod() failed";
        qvi_log_error("{} {}", ers, strerror(err));
        return QV_ERR_FILE_IO;
    }
    *fd = ifd;
    return QV_SUCCESS;
}

int
qvi_hwloc::topology_export(
    const std::string &base_path,
    std::string &path
) {
    int qvrc = QV_SUCCESS, rc = 0, fd = 0;
    cstr_t ers = nullptr;
    char *topo_xml = nullptr;

    do {
        int err = 0;
        const bool usable = qvi_access(base_path, R_OK | W_OK, &err);
        if (qvi_unlikely(!usable)) {
            ers = "Cannot export hardware topology to {} ({})";
            qvi_log_error(ers, base_path, strerror(err));
            return QV_ERR;
        }

        int topo_xml_len = 0;
        rc = hwloc_topology_export_xmlbuffer(
            m_topo, &topo_xml, &topo_xml_len,
            0 // We don't need 1.x compatible XML export.
        );
        if (qvi_unlikely(rc == -1)) {
            ers = "hwloc_topology_export_xmlbuffer() failed";
            qvrc = QV_ERR_HWLOC;
            break;
        }

        path = m_topo_file = topo_fname(base_path);

        qvrc = s_topo_fopen(m_topo_file.c_str(), &fd);
        if (qvi_unlikely(qvrc != QV_SUCCESS)) {
            ers = "topo_fopen() failed";
            break;
        }

        rc = write(fd, topo_xml, topo_xml_len);
        if (qvi_unlikely(rc == -1 || rc != topo_xml_len)) {
            const int err = errno;
            ers = "write() failed";
            qvi_log_error("{} {}", ers, strerror(err));
            qvrc = QV_ERR_FILE_IO;
            break;
        }
    } while (false);

    if (qvi_unlikely(ers)) {
        qvi_log_error("{} with rc={} ({})", ers, qvrc, qv_strerr(qvrc));
    }
    hwloc_free_xmlbuffer(m_topo, topo_xml);
    (void)close(fd);
    return qvrc;
}

hwloc_topology_t
qvi_hwloc::topology_get(void)
{
    return m_topo;
}

bool
qvi_hwloc::topology_is_this_system(void)
{
    return !!hwloc_topology_is_thissystem(topology_get());
}

hwloc_const_cpuset_t
qvi_hwloc::topology_get_cpuset(void)
{
    return hwloc_topology_get_allowed_cpuset(m_topo);
}

hwloc_const_cpuset_t
qvi_hwloc::topology_get_disallowed_cpuset(void)
{
    return hwloc_topology_get_topology_cpuset(m_topo);
}

hwloc_obj_type_t
qvi_hwloc::obj_get_type(
    qv_hw_obj_type_t external
) {
    switch(external) {
        case(QV_HW_OBJ_MACHINE):
            return HWLOC_OBJ_MACHINE;
        case(QV_HW_OBJ_PACKAGE):
            return HWLOC_OBJ_PACKAGE;
        case(QV_HW_OBJ_CORE):
            return HWLOC_OBJ_CORE;
        case(QV_HW_OBJ_PU):
            return HWLOC_OBJ_PU;
        case(QV_HW_OBJ_L1CACHE):
            return HWLOC_OBJ_L1CACHE;
        case(QV_HW_OBJ_L2CACHE):
            return HWLOC_OBJ_L2CACHE;
        case(QV_HW_OBJ_L3CACHE):
            return HWLOC_OBJ_L3CACHE;
        case(QV_HW_OBJ_L4CACHE):
            return HWLOC_OBJ_L4CACHE;
        case(QV_HW_OBJ_L5CACHE):
            return HWLOC_OBJ_L5CACHE;
        case(QV_HW_OBJ_NUMANODE):
            return HWLOC_OBJ_NUMANODE;
        case(QV_HW_OBJ_GPU):
            return HWLOC_OBJ_OS_DEVICE;
        default:
            // This is likely an internal development error.
            throw qvi_runtime_error(QV_ERR_INTERNAL);
    }
}

bool
qvi_hwloc::obj_is_host_resource(
    qv_hw_obj_type_t type
) {
    switch(type) {
        case(QV_HW_OBJ_MACHINE):
        case(QV_HW_OBJ_PACKAGE):
        case(QV_HW_OBJ_CORE):
        case(QV_HW_OBJ_PU):
        case(QV_HW_OBJ_L1CACHE):
        case(QV_HW_OBJ_L2CACHE):
        case(QV_HW_OBJ_L3CACHE):
        case(QV_HW_OBJ_L4CACHE):
        case(QV_HW_OBJ_L5CACHE):
        case(QV_HW_OBJ_NUMANODE):
            return true;
        case(QV_HW_OBJ_GPU):
        case(QV_HW_OBJ_LAST):
            return false;
        default:
            // This is likely an internal development error.
            throw qvi_runtime_error(QV_ERR_INTERNAL);
    }
}

int
qvi_hwloc::obj_type_depth(
    qv_hw_obj_type_t type,
    int *depth
) {
    const hwloc_obj_type_t obj_type = qvi_hwloc::obj_get_type(type);
    *depth = hwloc_get_type_depth(m_topo, obj_type);
    return QV_SUCCESS;
}

int
qvi_hwloc::m_get_logical_bind_string(
    hwloc_const_bitmap_t bitmap,
    std::string &result
) {
    result.clear();

    const int num_pus = hwloc_get_nbobjs_inside_cpuset_by_type(
        m_topo, bitmap, HWLOC_OBJ_PU
    );

    hwloc_obj_t obj_pu = nullptr;
    qvi_hwloc_bitmap logical_bitmap;
    for (int i = 0; i < num_pus; ++i) {
        obj_pu = hwloc_get_next_obj_inside_cpuset_by_type(
            m_topo, bitmap, HWLOC_OBJ_PU, obj_pu
        );
        (void)hwloc_bitmap_set(logical_bitmap.data(), obj_pu->logical_index);
    }

    result.append("L");
    result.append(qvi_hwloc::bitmap_list_string(logical_bitmap.cdata()));

    return QV_SUCCESS;
}

int
qvi_hwloc::m_get_physical_bind_string(
    hwloc_const_bitmap_t bitmap,
    std::string &result
) {
    result.clear();

    result.append("P");
    result.append(qvi_hwloc::bitmap_list_string(bitmap));

    return QV_SUCCESS;
}

int
qvi_hwloc::bind_string(
    hwloc_const_bitmap_t bitmap,
    qv_bind_string_flags_t flags,
    char **result
) {
    *result = nullptr;

    int rc = QV_SUCCESS;
    const std::string sep = " ; ";
    std::string lresult;
    std::string presult;
    std::string iresult;

    if (flags & QV_BIND_STRING_LOGICAL) {
        rc = m_get_logical_bind_string(bitmap, lresult);
        if (qvi_unlikely(rc != QV_SUCCESS)) return rc;

        iresult.append(lresult);
    }
    if (flags & QV_BIND_STRING_PHYSICAL) {
        rc = m_get_physical_bind_string(bitmap, presult);
        if (qvi_unlikely(rc != QV_SUCCESS)) return rc;

        if (!iresult.empty()) {
            iresult.append(sep);
        }
        iresult.append(presult);
    }
    // Sanity check. This means that no valid flag paths were taken. This
    // likely means that an invalid flag was provided to the function.
    if (qvi_unlikely(iresult.empty())) {
        return QV_ERR_INVLD_ARG;
    }
    //
    const int np = asprintf(result, "%s", iresult.c_str());
    if (qvi_unlikely(np == -1)) {
        *result = nullptr;
        return QV_ERR_OOR;
    }
    return QV_SUCCESS;
}

int
qvi_hwloc::m_set_general_device_info(
    hwloc_obj_t obj,
    hwloc_obj_t pci_obj,
    const std::string &pci_bus_id,
    qvi_hwloc_device *device
) {
    switch (obj->attr->osdev.type) {
        // For our purposes a HWLOC_OBJ_OSDEV_COPROC
        // is the same as a HWLOC_OBJ_OSDEV_GPU.
        case HWLOC_OBJ_OSDEV_GPU:
        case HWLOC_OBJ_OSDEV_COPROC:
            device->type = QV_HW_OBJ_GPU;
            break;
        default:
            return QV_SUCCESS;
    }
    // Set vendor ID.
    device->vendor_id = pci_obj->attr->pcidev.vendor_id;
    // Save device name.
    device->name = std::string(obj->name);
    // Set the PCI bus ID.
    device->pci_bus_id = pci_bus_id;
    // Set visible device ID, if applicable.
    return set_visdev_id(device);
}

int
qvi_hwloc::m_set_gpu_device_info(
    hwloc_obj_t obj,
    qvi_hwloc_device *device
) {
    int id = qvi_hwloc_device::INVISIBLE_ID;
    //
    if (sscanf(obj->name, "rsmi%d", &id) == 1) {
        device->smi = id;
        device->uuid = std::string(hwloc_obj_get_info_by_name(obj, "AMDUUID"));
        return qvi_hwloc_rsmi_get_device_cpuset_by_device_id(
            this, device->smi, device->affinity
        );
    }
    //
    if (sscanf(obj->name, "nvml%d", &id) == 1) {
        device->smi = id;
        device->uuid = std::string(hwloc_obj_get_info_by_name(obj, "NVIDIAUUID"));
        return qvi_hwloc_nvml_get_device_cpuset_by_pci_bus_id(
            this, device->pci_bus_id, device->affinity
        );
    }
    return QV_SUCCESS;
}

int
qvi_hwloc::m_set_of_device_info(
    hwloc_obj_t obj,
    qvi_hwloc_device *device
) {
    // TODO(skg) Get cpuset, if available.
    device->uuid = std::string(hwloc_obj_get_info_by_name(obj, "NodeGUID"));
    return QV_SUCCESS;
}

/**
 * First pass: discover devices that must be added to the list of devices.
 */
int
qvi_hwloc::m_discover_devices_first_pass(void)
{
    hwloc_obj_t obj = nullptr;
    while ((obj = hwloc_get_next_osdev(m_topo, obj)) != nullptr) {
        const hwloc_obj_osdev_type_t type = obj->attr->osdev.type;
        // Consider only what is listed here.
        if (type != HWLOC_OBJ_OSDEV_GPU &&
            type != HWLOC_OBJ_OSDEV_COPROC &&
            type != HWLOC_OBJ_OSDEV_OPENFABRICS) {
            continue;
        }
        // Try to get the PCI object.
        std::string busid;
        hwloc_obj_t pci_obj = get_pci_busid(obj, busid);
        if (!pci_obj) continue;
        // Have we seen this device already? For example, opencl0d0 and cuda0
        // may correspond to the same GPU hardware.
        // insert().second returns whether or not item insertion took place. If
        // true, we have not seen it.
        const bool seen = !m_device_ids.insert(busid).second;
        if (seen) continue;
        // Add a new device with a unique PCI busid.
        auto new_dev = std::make_shared<qvi_hwloc_device>();
        // Save general device info to new device instance.
        const int rc = m_set_general_device_info(
            obj, pci_obj, busid, new_dev.get()
        );
        if (qvi_unlikely(rc != QV_SUCCESS)) return rc;
        // Add the new device to our list of available devices.
        m_devices.push_back(new_dev);
    }
    return QV_SUCCESS;
}

int
qvi_hwloc::m_discover_gpu_devices(void)
{
#ifndef QV_GPU_SUPPORT
    return QV_SUCCESS;
#endif
    // This will maintain a mapping of PCI bus ID to device pointers.
    qvi_hwloc_dev_map devmap;

    hwloc_obj_t obj = nullptr;
    while ((obj = hwloc_get_next_osdev(m_topo, obj)) != nullptr) {
        const hwloc_obj_osdev_type_t type = obj->attr->osdev.type;
        // Consider only what is listed here.
        if (type != HWLOC_OBJ_OSDEV_GPU &&
            type != HWLOC_OBJ_OSDEV_COPROC) {
            continue;
        }
        // Try to get the PCI object.
        std::string busid;
        hwloc_obj_t pci_obj = get_pci_busid(obj, busid);
        if (!pci_obj) continue;

        for (auto &dev : m_devices) {
            // Skip invisible devices.
            if (dev->id == qvi_hwloc_device::INVISIBLE_ID) continue;
            // Skip if this is not the PCI bus ID we are looking for.
            if (dev->pci_bus_id != busid) continue;
            // Set as much device info as we can. If device support isn't
            // available, then just return success.
            int rc = m_set_gpu_device_info(obj, dev.get());
            if (rc == QV_ERR_NOT_SUPPORTED) return QV_SUCCESS;
            else if (rc != QV_SUCCESS) return rc;
            // First, determine if this is a new device?
            auto got = devmap.find(busid);
            // New device (i.e., a new PCI bus ID)
            if (got == devmap.end()) {
                auto gpudev = std::make_shared<qvi_hwloc_device>();
                // Set base info from current device.
                rc = qvi_copy(*dev.get(), gpudev.get());
                if (qvi_unlikely(rc != QV_SUCCESS)) return rc;
                // Add it to our collection of 'seen' devices.
                devmap.insert({busid, gpudev});
            }
            // A device we have seen before. Try to set as much info as
            // possible. Note that we don't copy because we don't know if the
            // source device has a different amount of information.
            else {
                qvi_hwloc_device *gpudev = got->second.get();
                rc = m_set_gpu_device_info(obj, gpudev);
                if (qvi_unlikely(rc != QV_SUCCESS)) return rc;
            }
        }
    }
    // Now populate our GPU device list.
    for (const auto &mapi : devmap) {
        auto gpudev = std::make_shared<qvi_hwloc_device>();
        // Copy device info.
        const int rc = qvi_copy(*mapi.second.get(), gpudev.get());
        if (qvi_unlikely(rc != QV_SUCCESS)) return rc;
        // Save copy.
        m_gpus.push_back(gpudev);
    }
    // Sort list based on device ID.
    std::sort(m_gpus.begin(), m_gpus.end(), dev_list_compare_by_visdev_id);
    return QV_SUCCESS;
}

int
qvi_hwloc::m_discover_nic_devices(void)
{
    hwloc_obj_t obj = nullptr;
    while ((obj = hwloc_get_next_osdev(m_topo, obj)) != nullptr) {
        if (obj->attr->osdev.type != HWLOC_OBJ_OSDEV_OPENFABRICS) continue;
        // Try to get the PCI object.
        std::string busid;
        hwloc_obj_t pci_obj = get_pci_busid(obj, busid);
        if (!pci_obj) continue;

        for (auto &dev : m_devices) {
            // Skip if this is not the PCI bus ID we are looking for.
            if (dev->pci_bus_id != busid) continue;
            //
            int rc = m_set_of_device_info(obj, dev.get());
            if (qvi_unlikely(rc != QV_SUCCESS)) return rc;
            //
            auto nicdev = std::make_shared<qvi_hwloc_device>();
            rc = qvi_copy(*dev.get(), nicdev.get());
            if (qvi_unlikely(rc != QV_SUCCESS)) return rc;
            m_nics.push_back(nicdev);
        }
    }
    return QV_SUCCESS;
}

int
qvi_hwloc::m_discover_devices(void)
{
    int rc = m_discover_devices_first_pass();
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;
    rc = m_discover_gpu_devices();
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;
    return m_discover_nic_devices();
}

int
qvi_hwloc::get_nobjs_by_type(
   qv_hw_obj_type_t target_type,
   int *out_nobjs
) {
    int depth = HWLOC_TYPE_DEPTH_UNKNOWN;
    const int rc = obj_type_depth(target_type, &depth);
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;

    *out_nobjs = hwloc_get_nbobjs_by_depth(m_topo, depth);
    return rc;
}

int
qvi_hwloc::m_get_proc_cpubind(
    pid_t who,
    qvi_hwloc_bitmap &result
) {
    int rc = hwloc_get_proc_cpubind(
        m_topo, who, result.data(), HWLOC_CPUBIND_THREAD
    );
    if (qvi_unlikely(rc != 0)) return QV_ERR_HWLOC;
    // Note in some instances I've noticed that the system's topology cpuset is
    // a strict subset of a process' cpuset, so protect against that case by
    // setting the process' cpuset to the system topology's if the above case is
    // what we are dealing.
    hwloc_const_cpuset_t tcpuset = topology_get_cpuset();
    // If the topology's cpuset is less than the process' cpuset, adjust.
    rc = hwloc_bitmap_compare(tcpuset, result.cdata());
    if (qvi_unlikely(rc == -1)) {
        return result.set(tcpuset);
    }
    // Else just use the one that was gathered above.
    return QV_SUCCESS;
}

int
qvi_hwloc::task_get_cpubind(
    pid_t who,
    qvi_hwloc_bitmap &result
) {
    return m_get_proc_cpubind(who, result);
}

int
qvi_hwloc::task_set_cpubind_from_cpuset(
    pid_t task_id,
    hwloc_const_cpuset_t cpuset
) {
    const int rc = hwloc_set_proc_cpubind(
        m_topo, task_id, cpuset, HWLOC_CPUBIND_THREAD
    );
    if (qvi_unlikely(rc == -1)) return QV_ERR_NOT_SUPPORTED;
    return QV_SUCCESS;
}

int
qvi_hwloc::m_obj_get_by_type(
    qv_hw_obj_type_t type,
    int type_index,
    hwloc_obj_t *obj
) {
    const hwloc_obj_type_t obj_type = qvi_hwloc::obj_get_type(type);
    hwloc_obj_t iobj = hwloc_get_obj_by_type(
        m_topo, obj_type, (uint_t)type_index
    );
    if (qvi_unlikely(!iobj)) {
        // There are a couple of reasons why target_obj may be NULL. If this
        // ever happens and the specified type and obj index are valid, then
        // improve this code.
        qvi_log_error(
            "m_obj_get_by_type() failed. Please submit a bug report."
        );
        *obj = nullptr;
        return QV_ERR_INTERNAL;
    }
    *obj = iobj;
    return QV_SUCCESS;
}

/**
 *
 */
int
qvi_hwloc::m_task_obj_xop_by_type_id(
    qv_hw_obj_type_t type,
    pid_t task_id,
    int type_index,
    task_xop_obj_id opid,
    int *result
) {
    hwloc_obj_t obj;
    int rc = m_obj_get_by_type(type, type_index, &obj);
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;

    qvi_hwloc_bitmap cur_bind;
    rc = task_get_cpubind(task_id, cur_bind);
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;

    switch (opid) {
        case TASK_INTERSECTS_OBJ:
            *result = hwloc_bitmap_intersects(cur_bind.cdata(), obj->cpuset);
            break;
        case TASK_INCLUDEDIN_OBJ:
            *result = hwloc_bitmap_isincluded(cur_bind.cdata(), obj->cpuset);
            break;
        default:
            // Something bad happened to get here.
            qvi_abort();
    }
    return QV_SUCCESS;
}

int
qvi_hwloc::task_intersects_obj_by_type_id(
    qv_hw_obj_type_t type,
    pid_t task_id,
    int type_index,
    int *result
) {
    return m_task_obj_xop_by_type_id(
        type, task_id, type_index, TASK_INTERSECTS_OBJ, result
    );
}

int
qvi_hwloc::task_includedin_obj_by_type_id(
    qv_hw_obj_type_t type,
    pid_t task_id,
    int type_index,
    int *result
) {
    return m_task_obj_xop_by_type_id(
        type, task_id, type_index, TASK_INCLUDEDIN_OBJ, result
    );
}

int
qvi_hwloc::m_get_nosdevs_in_cpuset(
    const qvi_hwloc_dev_list &devs,
    hwloc_const_cpuset_t cpuset,
    int *nobjs
) {
    int ndevs = 0;
    for (const auto &dev : devs) {
        if (hwloc_bitmap_isincluded(dev->affinity.cdata(), cpuset)) ndevs++;
    }
    *nobjs = ndevs;

    return QV_SUCCESS;
}

int
qvi_hwloc::m_get_nobjs_in_cpuset(
    qv_hw_obj_type_t target_obj,
    hwloc_const_cpuset_t cpuset,
    int *nobjs
) {
    int depth;
    int rc = obj_type_depth(target_obj, &depth);
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;

    int n = 0;
    hwloc_obj_t obj = nullptr;
    while ((obj = hwloc_get_next_obj_by_depth(m_topo, depth, obj))) {
        if (!hwloc_bitmap_isincluded(obj->cpuset, cpuset)) continue;
        // Ignore objects with empty sets (can happen when outside of cgroup).
        if (hwloc_bitmap_iszero(obj->cpuset)) continue;
        n++;
    }
    *nobjs = n;
    return QV_SUCCESS;
}

int
qvi_hwloc::get_nobjs_in_cpuset(
    qv_hw_obj_type_t target_obj,
    hwloc_const_cpuset_t cpuset,
    int *nobjs
) {
    switch (target_obj) {
        case(QV_HW_OBJ_GPU) :
            return m_get_nosdevs_in_cpuset(m_gpus, cpuset, nobjs);
        default:
            return m_get_nobjs_in_cpuset(target_obj, cpuset, nobjs);
    }
}

int
qvi_hwloc::get_obj_type_in_cpuset(
    int npieces,
    hwloc_const_cpuset_t cpuset,
    qv_hw_obj_type_t *target_obj
) {
    const hwloc_topology_t topo = m_topo;
    hwloc_obj_t obj =  hwloc_get_first_largest_obj_inside_cpuset(topo, cpuset);
    int num = hwloc_get_nbobjs_inside_cpuset_by_type(topo, cpuset, obj->type);

    int depth = obj->depth;
    while(!((num > npieces) && (!hwloc_obj_type_is_cache(obj->type)))){
        depth++;
        obj = hwloc_get_obj_inside_cpuset_by_depth(topo, cpuset, depth, 0);
        num = hwloc_get_nbobjs_inside_cpuset_by_type(topo, cpuset, obj->type);
    }

    *target_obj = obj_hwloc2qv(obj->type);
    return QV_SUCCESS;
}

int
qvi_hwloc::get_obj_in_cpuset_by_depth(
    hwloc_const_cpuset_t cpuset,
    int depth,
    int index,
    hwloc_obj_t *result_obj
) {
    const hwloc_topology_t topo = m_topo;

    bool found = false;
    int i = 0;
    hwloc_obj_t obj = nullptr;
    while ((obj = hwloc_get_next_obj_by_depth(topo, depth, obj))) {
        if (!hwloc_bitmap_isincluded(obj->cpuset, cpuset)) continue;
        // Ignore objects with empty sets (can happen when outside of cgroup).
        if (hwloc_bitmap_iszero(obj->cpuset)) continue;
        if (i == index) {
            *result_obj = obj;
            found = true;
            break;
        }
        i++;
    }
    return (found ? QV_SUCCESS : QV_ERR_HWLOC);
}

const std::vector<qv_hw_obj_type_t> &
qvi_hwloc::supported_devices(void) {
    static const std::vector<qv_hw_obj_type_t> supported_devices = {
        QV_HW_OBJ_GPU
    };
    return supported_devices;
}

int
qvi_hwloc::devices_emit(
    qv_hw_obj_type_t obj_type
) {
    qvi_hwloc_dev_list *devlist = nullptr;
    switch (obj_type) {
        case QV_HW_OBJ_GPU:
            devlist = &m_gpus;
            break;
        default:
            return QV_ERR_NOT_SUPPORTED;
    }
    for (auto &dev : *devlist) {
        const std::string cpusets = qvi_hwloc::bitmap_list_string(
            dev->affinity.cdata()
        );
        qvi_log_info("  Device Name: {}", dev->name);
        qvi_log_info("  Device PCI Bus ID: {}", dev->pci_bus_id);
        qvi_log_info("  Device UUID: {}", dev->uuid);
        qvi_log_info("  Device affinity: {}", cpusets);
        qvi_log_info("  Device Vendor ID: {}", dev->vendor_id);
        qvi_log_info("  Device SMI: {}", dev->smi);
        qvi_log_info("  Device Visible Device ID: {}\n", dev->id);
    }
    return QV_SUCCESS;
}

static int
get_devices_in_cpuset_from_dev_list(
    const qvi_hwloc_dev_list &devlist,
    hwloc_const_cpuset_t cpuset,
    qvi_hwloc_dev_list &devs
) {
    for (auto &dev : devlist) {
        if (!hwloc_bitmap_isincluded(dev->affinity.cdata(), cpuset)) continue;

        auto devin = std::make_shared<qvi_hwloc_device>();
        const int rc = qvi_copy(*dev.get(), devin.get());
        if (rc != QV_SUCCESS) return rc;
        devs.push_back(devin);
    }
    return QV_SUCCESS;
}

int
qvi_hwloc::get_devices_in_cpuset(
    qv_hw_obj_type_t obj_type,
    hwloc_const_cpuset_t cpuset,
    qvi_hwloc_dev_list &devs
) {
    qvi_hwloc_dev_list *devlist = nullptr;
    // Make sure that the user provided a valid, supported device type.
    switch (obj_type) {
        // TODO(skg) We can support more devices, so do that eventually.
        case QV_HW_OBJ_GPU:
            devlist = &m_gpus;
            break;
        default:
            return QV_ERR_NOT_SUPPORTED;
    }

    return get_devices_in_cpuset_from_dev_list(
        *devlist, cpuset, devs
    );
}

int
qvi_hwloc::get_device_id_in_cpuset(
    qv_hw_obj_type_t dev_obj,
    int i,
    hwloc_const_cpuset_t cpuset,
    qv_device_id_type_t dev_id_type,
    std::string &dev_id
) {
    qvi_hwloc_dev_list devs;
    int rc = get_devices_in_cpuset(dev_obj, cpuset, devs);
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;

    switch (dev_id_type) {
        case (QV_DEVICE_ID_UUID):
            dev_id = devs.at(i)->uuid;
            break;
        case (QV_DEVICE_ID_PCI_BUS_ID):
            dev_id = devs.at(i)->pci_bus_id;
            break;
        case (QV_DEVICE_ID_ORDINAL):
            dev_id = devs.at(i)->id;
            break;
        default:
            rc = QV_ERR_INVLD_ARG;
            break;
    }
    return rc;
}

int
qvi_hwloc::m_split_cpuset_chunk_size(
    hwloc_const_cpuset_t cpuset,
    uint_t npieces,
    uint_t *chunk_size
) {
    int npus = 0;
    const int rc = m_get_nobjs_in_cpuset(
        QV_HW_OBJ_PU, cpuset, &npus
    );
    if (rc != QV_SUCCESS || npieces == 0 || npus == 0 ) {
        *chunk_size = 0;
    }
    else {
        *chunk_size = npus / npieces;
    }
    return rc;
}

int
qvi_hwloc::m_split_cpuset_by_range(
    hwloc_const_cpuset_t cpuset,
    uint_t base,
    uint_t extent,
    hwloc_bitmap_t result
) {
    // Zero-out the result bitmap that will encode the split.
    hwloc_bitmap_zero(result);
    // We use PUs to split resources. Each set bit represents a PU. The number
    // of bits set represents the number of PUs present on the system. The
    // least-significant (right-most) bit represents logical ID 0.
    int pu_depth = 0;
    int rc = obj_type_depth(QV_HW_OBJ_PU, &pu_depth);
    if (rc != QV_SUCCESS) return rc;
    // Calculate split based on given range.
    for (uint_t i = base; i < base + extent; ++i) {
        hwloc_obj_t dobj;
        rc = get_obj_in_cpuset_by_depth(cpuset, pu_depth, i, &dobj);
        if (rc != QV_SUCCESS) break;

        const int orrc = hwloc_bitmap_or(result, result, dobj->cpuset);
        if (orrc != 0) {
            rc = QV_ERR_HWLOC;
            break;
        }
    }
    return rc;
}

int
qvi_hwloc::get_cpuset_for_nobjs(
    hwloc_const_cpuset_t cpuset,
    qv_hw_obj_type_t obj_type,
    uint_t nobjs,
    qvi_hwloc_bitmap &result
) {
    // Get the target object's depth.
    int obj_depth;
    int rc = obj_type_depth(obj_type, &obj_depth);
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;
    // Calculate cpuset based on number of desired objects.
    for (uint_t i = 0; i < nobjs; ++i) {
        hwloc_obj_t dobj;
        rc = get_obj_in_cpuset_by_depth(
            cpuset, obj_depth, i, &dobj
        );
        if (rc != QV_SUCCESS) break;

        const int orrc = hwloc_bitmap_or(
            result.data(), result.cdata(), dobj->cpuset
        );
        if (orrc != 0) {
            rc = QV_ERR_HWLOC;
            break;
        }
    }
    return rc;
}

qvi_hwloc_bitmap
qvi_hwloc::bitmap_disable_smt(
    const qvi_hwloc_bitmap &bitmap
) {
    qvi_hwloc_bitmap nosmt_bitmap;
    // How many cores are in the unmodified bitmap?
    const unsigned ncores = hwloc_get_nbobjs_inside_cpuset_by_type(
        m_topo, bitmap.cdata(), HWLOC_OBJ_CORE
    );

    for (unsigned i = 0; i < ncores; ++i) {
        hwloc_obj_t obj = hwloc_get_obj_inside_cpuset_by_type(
            m_topo, bitmap.cdata(), HWLOC_OBJ_CORE, i
        );
        qvi_hwloc_bitmap core_cpuset(obj->cpuset);
        hwloc_bitmap_singlify(core_cpuset.data());
        hwloc_bitmap_or(
            nosmt_bitmap.data(), nosmt_bitmap.cdata(), core_cpuset.cdata()
        );
    }
    return nosmt_bitmap;
}

int
qvi_hwloc::get_device_affinity(
    qv_hw_obj_type_t dev_obj,
    int device_id,
    qvi_hwloc_bitmap &cpuset
) {
    qvi_hwloc_dev_list *devlist = nullptr;

    switch(dev_obj) {
        case(QV_HW_OBJ_GPU) :
            devlist = &m_gpus;
            break;
        default:
            return QV_ERR_NOT_SUPPORTED;
    }
    // XXX(skg) This isn't the most efficient way of doing this, but the device
    // lists tend to be small, so just perform a linear search for the given ID.
    for (const auto &dev : *devlist) {
        if (dev->id != device_id) continue;
        return cpuset.set(dev->affinity.cdata());
    }
    return QV_ERR_NOT_FOUND;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
