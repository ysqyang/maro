# Copyright (c) Microsoft Corporation.
# Licensed under the MIT license.

#cython: language_level=3
#distutils: language = c++

from cpython cimport bool


cdef int raise_get_attr_error() except +:
    raise Exception("Bad parameters to get attribute value.")


cdef class SnapshotListAbc:
    cdef query(self, IDENTIFIER node_id, list ticks, list node_index_list, list attr_list) except +:
        pass

    cdef void take_snapshot(self, INT tick) except +:
        pass

    cdef USHORT get_node_number(self, IDENTIFIER node_id) except +:
        return 0

    cdef USHORT get_slots_number(self, IDENTIFIER attr_id) except +:
        return 0

    cdef void enable_history(self, str history_folder) except +:
        pass

    cdef void reset(self) except +:
        pass

    cdef list get_frame_index_list(self) except +:
        return []


cdef class BackendAbc:

    cdef bool is_support_dynamic_features(self):
        return False

    cdef IDENTIFIER add_node(self, str name, NODE_INDEX number) except +:
        pass

    cdef IDENTIFIER add_attr(self, IDENTIFIER node_id, str attr_name, str dtype, SLOT_INDEX slot_num) except +:
        pass

    cdef void set_attr_value(self, NODE_INDEX node_index, IDENTIFIER attr_id, SLOT_INDEX slot_index, object value) except +:
        pass

    cdef object get_attr_value(self, NODE_INDEX node_index, IDENTIFIER attr_id, SLOT_INDEX slot_index) except +:
        pass

    cdef void set_attr_values(self, NODE_INDEX node_index, IDENTIFIER attr_id, SLOT_INDEX[:] slot_index, list value) except +:
        pass

    cdef list get_attr_values(self, NODE_INDEX node_index, IDENTIFIER attr_id, SLOT_INDEX[:] slot_indices) except +:
        pass

    cdef void reset(self) except +:
        pass

    cdef void setup(self, bool enable_snapshot, USHORT total_snapshot, dict options) except +:
        pass

    cdef dict get_node_info(self) except +:
        return {}

    cdef void append_node(self, IDENTIFIER node_id, NODE_INDEX number) except +:
        pass

    cdef void delete_node(self, IDENTIFIER node_id, NODE_INDEX node_index) except +:
        pass

    cdef void resume_node(self, IDENTIFIER node_id, NODE_INDEX node_index) except +:
        pass

    cdef void set_attribute_slot(self, IDENTIFIER attr_id, SLOT_INDEX slots) except +:
        pass
