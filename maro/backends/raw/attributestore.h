// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#ifndef _MARO_BACKENDS_RAW_ATTRIBUTESTORE
#define _MARO_BACKENDS_RAW_ATTRIBUTESTORE

#include <vector>
#include <unordered_map>
#include <iterator>

#include "common.h"
#include "attribute.h"
#include "bitset.h"

namespace maro
{
  namespace backends
  {
    namespace raw
    {
      /// <summary>
      /// Attribute not exist exception.
      /// </summary>
      class BadAttributeIndexing : public exception
      {
        public:
          const char* what() const noexcept override;
      };

      /// <summary>
      /// Helper function to construct attribute key
      /// </summary>
      /// <param name="node_id">Id of node</param>
      /// <param name="node_index">Index of node</param>
      /// <param name="attr_id">Id of attribute</param>
      /// <param name="slot_index">Index of slot</param>
      /// <returns>Attribute key in store</returns>
      inline ULONG attr_index_key(IDENTIFIER node_id, NODE_INDEX node_index, IDENTIFIER attr_id, SLOT_INDEX slot_index);

      const USHORT LENGTH_PER_PART = sizeof(USHORT) * BITS_PER_BYTE;

      /*
      NOTE:
        1. removing will not change last index
        2. adding will increase last index
        3. arrange will update last index when filling the empty slots
      */

      /// <summary>
      /// Attribute store used to store attributes' value and related mapping
      /// </summary>
      class AttributeStore
      {
        // attribute mapping: [node_id, node_index, attr_id, slot_index] -> attribute index
        unordered_map<ULONG, size_t> _mapping;

        // attribute index -> [node_id, node_index, attr_id, slot_index]
        unordered_map<size_t, ULONG> _i2kmaping;

        vector<Attribute> _attributes;

        // Bitset for empty slot mask
        Bitset _slot_masks;

        // arrange if dirty
        bool _is_dirty{false};

        // index of last attribute
        size_t _last_index{0ULL};

      public:
        /// <summary>
        /// Setup attributes store.
        /// </summary>
        /// <param name="size">Initial size of store, this would be 64 times, it will be expend to times of 64 if not</param>
        void setup(size_t size);

        /// <summary>
        /// Arrange attribute store to avoid empty slots in the middle
        /// </summary>
        void arrange();

        /// <summary>
        /// Attribute getter to support get attribue like: auto& attr = store(node_id, node_index, attr_id, slot_index)
        /// </summary>
        /// <param name="node_id">IDENTIFIER of node</param>
        /// <param name="node_index">Index of node</param>
        /// <param name="attr_id">IDENTIFIER of attribute</param>
        /// <param name="slot_index">Slot of attribute</param>
        /// <returns>Attribute at specified place</returns>
        Attribute &operator()(IDENTIFIER node_id, NODE_INDEX node_index, IDENTIFIER attr_id, SLOT_INDEX slot_index = 0);

        /// <summary>
        /// Add nodes with its attribute, this function should be called for several times if node contains more than 1 attributes.
        /// </summary>
        /// <param name="node_id">Id of node</param>
        /// <param name="node_start_index">Start index of not to add</param>
        /// <param name="node_num">Number of nodes to add</param>
        /// <param name="attr_id">Id of attribute</param>
        /// <param name="slot_num">Number of slots</param>
        void add_nodes(IDENTIFIER node_id, NODE_INDEX node_start_index, NODE_INDEX stop, IDENTIFIER attr_id, SLOT_INDEX slot_num);

        /// <summary>
        /// Remove specified node
        /// </summary>
        /// <param name="node_id"></param>
        /// <param name="node_index"></param>
        /// <param name="attr_id"></param>
        /// <param name="slot_num"></param>
        void remove_node(IDENTIFIER node_id, NODE_INDEX node_index, IDENTIFIER attr_id, SLOT_INDEX slot_num);

        /// <summary>
        /// Remove specified range of attribute slots
        /// </summary>
        /// <param name="node_id"></param>
        /// <param name="node_num"></param>
        /// <param name="attr_id"></param>
        /// <param name="from"></param>
        /// <param name="stop"></param>
        void remove_attr_slots(IDENTIFIER node_id, NODE_INDEX node_num, IDENTIFIER attr_id, SLOT_INDEX from, SLOT_INDEX stop);

        /// <summary>
        /// Copy data to specified dest, this function will arrange internally before copy
        /// </summary>
        /// <param name="attr_dest">Dest of attributes to copy</param>
        /// <param name="attr_map">Dest of attribute mapping, pass nullptr to skip copy mapping</param>
        void copy_to(Attribute *attr_dest, unordered_map<ULONG, size_t> *attr_map);

        /// <summary>
        /// Size of current attributes
        /// </summary>
        /// <returns>Size of attributes</returns>
        size_t size();

        /// <summary>
        /// Reset all current attributes, this will clear all attributes and mapping
        /// </summary>
        void reset();

        /// <summary>
        /// Is there is empty slots in the middle
        /// </summary>
        /// <returns>True if has empty slots, or false</returns>
        bool is_dirty();

#ifdef _DEBUG
        size_t capacity();
        size_t last_index();
#endif

      private:
        /// <summary>
        /// Update last index after operations that will cause last index changed
        /// </summary>
        void update_last_index();
      };
    } // namespace raw
  }   // namespace backends
} // namespace maro

#endif
