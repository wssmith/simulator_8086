#ifndef WS_DECODER_HPP
#define WS_DECODER_HPP

#include <cstdint>
#include <span>

struct instruction;
enum class operation_type : uint32_t;

using data_iterator = std::span<uint8_t>::iterator;

instruction decode_instruction(data_iterator& data_iter, const data_iterator& data_end, uint32_t address);

char const* get_mneumonic(operation_type type);

#endif
