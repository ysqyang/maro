// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "snapshotlist.h"

namespace maro
{
  namespace backends
  {
    namespace raw
    {
      void SnapshotList::SnapshotQueryParameters::reset()
      {
        node_id = 0;

        ticks = nullptr;
        tick_length = 0;

        node_indices = nullptr;
        node_length = 0;

        attributes = nullptr;
        attr_length = 0;
      }

      void SnapshotList::set_frame(Frame *frame)
      {
        _frame = frame;
      }

      void SnapshotList::set_max_size(USHORT max_size)
      {
        if (max_size == 0)
        {
          throw InvalidSnapshotSize();
        }

        if (_max_size == 0)
        {
          _max_size = max_size;
        }
      }

      void SnapshotList::take_snapshot(INT tick, AttributeStore *frame_attr_store)
      {
        if (frame_attr_store == nullptr)
        {
          // try to use frame
          if (_frame == nullptr)
          {
            throw SnapshotInvalidFrameState();
          }

          frame_attr_store = &(_frame->_attr_store);
        }

        ensure_max_size();
        prepare_memory();

        // To make it easy to implement, we do not support over-write exist tick at any time,
        // tick can onlly be over-wrote if last one is same tick

        auto snapshot_size = frame_attr_store->size();

        // shall we skip the step to erase oldest tick? it will be true we deleted an existing tick
        bool skip_oldest_erase = false;

        {
          // 1. check tick exist
          auto tick_pair = _tick2index_map.find(tick);

          // tick exist
          if (tick_pair != _tick2index_map.end())
          {
            // then check if last tick is same
            if (_last_tick != tick)
            {
              throw InvalidSnapshotTick();
            }

            // for exist tick, it has 2 situation
            // 1. at the end:
            //    we can just set _end_index to start of this tick
            // 2. just before empty slots
            //    we just move first empty slot index to start of this tick, and its length to empty slots length

            auto exist_tick_index = tick_pair->second;
            auto exist_tick_length = _tick2size_map.find(tick)->second;

            // remove info about this tick
            _tick2index_map.erase(tick);
            _tick2size_map.erase(tick);
            _tick_attr_map.erase(tick);

            // is this tick at the end?
            if (exist_tick_index + exist_tick_length == _end_index)
            {
              _end_index = exist_tick_index;
            }
            else
            {
              _first_empty_slot_index = exist_tick_index;
              _empty_slots_length += exist_tick_length;
            }

            _cur_snapshot_num--;

            skip_oldest_erase = true;
          }
        }

        _cur_snapshot_num++;

        if (_cur_snapshot_num > _max_size)
        {
          // Do overlap
          // skip means we have delete a tick before (existing tick), so we do not need to delete oldest one here
          if (!skip_oldest_erase)
          {
            // find oldest tick to delete
            auto oldest_item = _tick2index_map.begin();
            auto oldest_tick = oldest_item->first;
            auto oldest_index = oldest_item->second;
            auto oldest_size = _tick2size_map.find(oldest_tick)->second;

            /// remove from mappings
            _tick2index_map.erase(oldest_tick);
            _tick2size_map.erase(oldest_tick);
            _tick_attr_map.erase(oldest_tick);

            // update empty slots area flags

            // if not empty slots in the middle, then use current as first
            if (_empty_slots_length == 0)
            {
              _first_empty_slot_index = oldest_index;
              _empty_slots_length = oldest_size;
            }
            else
            {
              // or it must be right after current empty slots
              _empty_slots_length += oldest_size;
            }
          }
          // if remaining empty slots enough?
          if (_empty_slots_length >= snapshot_size)
          {
            write_to_empty_slots(frame_attr_store, tick);
          }
          else
          {
            // append to the end
            append_to_end(frame_attr_store, tick);
          }
        }
        else
        {
          // append
          append_to_end(frame_attr_store, tick);
        }

        _last_tick = tick;
      }

      Attribute &SnapshotList::operator()(INT tick, IDENTIFIER node_id, NODE_INDEX node_index, IDENTIFIER attr_id, SLOT_INDEX slot_index)
      {
        auto tick_index_pair = _tick2index_map.find(tick);

        if (tick_index_pair == _tick2index_map.end())
        {
          // return NAN if not exist
          return _defaultAttr;
        }

        auto tick_start_index = tick_index_pair->second;
        auto &mapping = _mappings[_tick_attr_map.find(tick)->second];

        auto key = attr_index_key(node_id, node_index, attr_id, slot_index);

        auto offset_pair = mapping.find(key);

        if (offset_pair == mapping.end())
        {
          return _defaultAttr;
        }

        auto offset = offset_pair->second;

        return _attr_store[tick_start_index + offset];
      }

      USHORT SnapshotList::size()
      {
        return _cur_snapshot_num > _max_size ? _max_size : _cur_snapshot_num;
      }

      USHORT SnapshotList::max_size()
      {
        return _max_size;
      }

      void SnapshotList::reset()
      {
        _tick2index_map.clear();
        _tick2size_map.clear();
        _tick_attr_map.clear();
        _mappings.clear();

        memset(&_attr_store[0], 0, sizeof(Attribute) * _attr_store.size());

        _first_empty_slot_index = 0;
        _empty_slots_length = 0;
        _end_index = 0;
        _cur_snapshot_num = 0;
        _last_tick = -1;
        _is_prepared = false;
        _query_parameters.reset();
      }

      void SnapshotList::dump(string path)
      {
        for (auto node : _frame->_nodes)
        {
          auto full_path = path + "/" + "snapshots_" + node.name + ".csv";

          ofstream file(full_path);

          // write headers
          file << "tick,node_index";

          for (auto attr_id : _frame->_node_2_attrs.find(node.id)->second)
          {
            auto &attr_info = _frame->_attributes[attr_id];

            file << "," << attr_info.name;
          }

          file << "\n";

          // write contents
          for (auto tick_iter : _tick2index_map)
          {
            auto tick = tick_iter.first;

            for (NODE_INDEX node_index = 0; node_index < node.number; node_index++)
            {
              file << tick << "," << node_index;

              for (auto attr_id : _frame->_node_2_attrs.find(node.id)->second)
              {
                auto &attr_info = _frame->_attributes[attr_id];

                if (attr_info.max_slots == 1)
                {
                  file << ",";

                  // write one value
                  write_attribute(file, tick, node.id, node_index, attr_id, 0);
                }
                else
                {
                  file << ",\"[";

                  // write a list
                  for (SLOT_INDEX slot_index = 0; slot_index < attr_info.max_slots; slot_index++)
                  {
                    write_attribute(file, tick, node.id, node_index, attr_id, slot_index);

                    file << ",";
                  }

                  file << "]\"";
                }
              }

              file << "\n";
            }
          }

          file.close();
        }
      }

      void SnapshotList::get_ticks(INT *result)
      {
        auto i = 0;

        for (auto iter : _tick2index_map)
        {
          result[i] = iter.first;
          i++;
        }
      }

      SnapshotResultShape SnapshotList::prepare(IDENTIFIER node_id, INT ticks[], UINT tick_length, NODE_INDEX node_indices[],
                                                UINT node_length, IDENTIFIER attributes[], UINT attr_length)
      {
        // we do not support empty attribute
        if (attributes == nullptr)
        {
          throw SnapshotQueryNoAttributes();
        }

        ensure_max_size();

        if (_frame == nullptr)
        {
          throw SnapshotInvalidFrameState();
        }

        _frame->ensure_node_id(node_id);

        auto &node = _frame->_nodes[node_id];

        auto srs = SnapshotResultShape();

        // get max length of slot for all attribute
        for (UINT attr_index = 0; attr_index < attr_length; attr_index++)
        {
          auto &attr = _frame->_attributes[attributes[attr_index]];

          srs.max_slot_number = max(attr.max_slots, srs.max_slot_number);
        }

        // correct node number
        if (node_indices == nullptr)
        {
          node_length = node.number;
        }

        if (ticks == nullptr)
        {
          tick_length = _tick2index_map.size();
        }

        // Choose what we need
        // TODO: validate attributes
        _query_parameters.attributes = attributes;
        _query_parameters.attr_length = attr_length;
        _query_parameters.node_id = node_id;
        _query_parameters.node_indices = node_indices;
        _query_parameters.node_length = node_length;
        _query_parameters.ticks = ticks;
        _query_parameters.tick_length = tick_length;

        _is_prepared = true;

        // fill others
        srs.max_node_number = node_length;
        srs.tick_number = tick_length;
        srs.attr_number = attr_length;

        return srs;
      }

      void SnapshotList::query(ATTR_FLOAT *result, SnapshotResultShape shape)
      {
        if (result == nullptr)
        {
          throw SnapshotQueryResultPtrNull();
        }

        // ensure prepare
        if (!_is_prepared)
        {
          throw SnapshotQueryNotPrepared();
        }

        // ensure shape not zero
        if (shape.attr_number == 0 || shape.max_node_number == 0 || shape.max_slot_number == 0 || shape.tick_number == 0)
        {
          return;
        }

        ensure_max_size();

        if (_frame == nullptr)
        {
          throw SnapshotInvalidFrameState();
        }

        auto node_id = _query_parameters.node_id;

        auto &node = _frame->_nodes[node_id];

        auto *ticks = _query_parameters.ticks;
        auto *node_indices = _query_parameters.node_indices;
        auto *attrs = _query_parameters.attributes;
        auto tick_length = _query_parameters.tick_length;
        auto node_length = _query_parameters.node_length;
        auto attr_length = _query_parameters.attr_length;

        vector<INT> _ticks;

        // Prepare ticks if no one passed
        if (_query_parameters.ticks == nullptr)
        {
          tick_length = _tick2index_map.size();

          for (auto iter = _tick2index_map.begin(); iter != _tick2index_map.end(); iter++)
          {
            _ticks.push_back(iter->first);
          }
        }

        vector<NODE_INDEX> _node_indices;

        if (node_indices == nullptr)
        {
          node_length = node.number;

          for (auto i = 0; i < node.number; i++)
          {
            _node_indices.push_back(i);
          }
        }

        const INT *__ticks = ticks == nullptr ? &_ticks[0] : ticks;
        const NODE_INDEX *__node_indices = node_indices == nullptr ? &_node_indices[0] : node_indices;

        auto result_index = 0;

        for (UINT i = 0; i < tick_length; i++)
        {
          auto tick = __ticks[i];

          for (UINT j = 0; j < node_length; j++)
          {
            auto node_index = __node_indices[j];

            for (UINT k = 0; k < attr_length; k++)
            {
              auto attr_id = attrs[k];

              for (SLOT_INDEX slot_index = 0; slot_index < shape.max_slot_number; slot_index++)
              {
                auto &attr = operator()(tick, node_id, node_index, attr_id, slot_index);

                if (!attr.is_nan())
                {
                  result[result_index] = ATTR_FLOAT(attr);
                }

                result_index++;
              }
            }
          }
        }

        _is_prepared = false;

        // reset current query parameters
        _query_parameters.reset();
      }

      void SnapshotList::copy_from_attr_store(AttributeStore *frame_attr_store, INT tick, size_t start_index)
      {
        auto last_tick_map = _tick_attr_map.rbegin();
        auto is_copy_mapping = false;

        // if attribute store is dirty means there is something changed, we should keep current mapping
        if (last_tick_map == _tick_attr_map.rend() || frame_attr_store->is_dirty())
        {
          is_copy_mapping = true;
        }
        else
        {
          auto &last_mapping = _mappings[last_tick_map->second];

          if (last_mapping.size() != frame_attr_store->size())
          {
            is_copy_mapping = true;
          }
        }

        if (is_copy_mapping)
        {
          _mappings.emplace_back();

          // copy
          auto &mapping = _mappings[_mappings.size() - 1];

          frame_attr_store->copy_to(&_attr_store[start_index], &mapping);

          _tick_attr_map[tick] = _mappings.size() - 1;
        }
        else
        {
          frame_attr_store->copy_to(&_attr_store[start_index], nullptr);

          _tick_attr_map[tick] = last_tick_map->second;
        }
      }

      void SnapshotList::append_to_end(AttributeStore *frame_attr_store, INT tick)
      {
        auto snapshot_size = frame_attr_store->size();

        // prepare attribute store to make sure we can hold all
        if (_end_index + snapshot_size > _attr_store.size())
        {
          _attr_store.resize((_end_index + snapshot_size) * 2);
        }

        // copy
        copy_from_attr_store(frame_attr_store, tick, _end_index);

        _tick2size_map[tick] = snapshot_size;
        _tick2index_map[tick] = _end_index;

        _end_index += snapshot_size;
      }

      void SnapshotList::write_to_empty_slots(AttributeStore *frame_attr_store, INT tick)
      {
        auto snapshot_size = frame_attr_store->size();

        // write to here
        copy_from_attr_store(frame_attr_store, tick, _first_empty_slot_index);

        _tick2index_map[tick] = _first_empty_slot_index;
        _tick2size_map[tick] = snapshot_size;

        _first_empty_slot_index += snapshot_size;
        _empty_slots_length -= snapshot_size;
      }

      inline void SnapshotList::ensure_max_size()
      {
        if (_max_size == 0)
        {
          throw InvalidSnapshotSize();
        }
      }

      inline void SnapshotList::write_attribute(ofstream &file, INT tick, IDENTIFIER node_id, NODE_INDEX node_index, IDENTIFIER attr_id, SLOT_INDEX slot_index)
      {
        auto &attr = operator()(tick, node_id, node_index, attr_id, slot_index);

        if (attr.is_nan())
        {
          file << "nan";
        }
        else
        {
          file << ATTR_FLOAT(attr);
        }
      }

      inline void SnapshotList::prepare_memory()
      {
        if (_frame != nullptr && _attr_store.size() == 0)
        {
          _attr_store.resize(_frame->_attr_store.size() * _max_size);
        }
      }

      const char* InvalidSnapshotTick::what() const noexcept
      {
        return "Invalid tick to take snapshot, same tick must be used sequentially.";
      }

      const char* InvalidSnapshotSize::what() const noexcept
      {
        return "Invalid snapshot list max size, it must be larger than 0.";
      }

      const char* SnapshotQueryNotPrepared::what() const noexcept
      {
        return "Query must be after prepare function.";
      }

      const char* SnapshotQueryNoAttributes::what() const noexcept
      {
        return "Attribute list for query should contain at least 1.";
      }

      const char* SnapshotInvalidFrameState::what() const noexcept
      {
        return "Not set frame before operations.";
      }

      const char* SnapshotQueryResultPtrNull::what() const noexcept
      {
        return "Result pointer is NULL.";
      }

#ifdef _DEBUG
      pair<size_t, size_t> SnapshotList::empty_states()
      {
        return make_pair(_first_empty_slot_index, _empty_slots_length);
      }

      size_t SnapshotList::end_index()
      {
        return _end_index;
      }
#endif
    } // namespace raw
  }   // namespace backends
} // namespace maro