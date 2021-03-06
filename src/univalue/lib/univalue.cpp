// Copyright 2014 BitPay Inc.
// Copyright 2015 Bitcoin Core Developers
// Copyright (c) 2020 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <algorithm>
#include <array>
#include <cinttypes>
#include <cmath>
#include <cstdio>
#include <iomanip>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <stdint.h>
#include <stdlib.h>
#include <type_traits>

#include "univalue.h"

const UniValue NullUniValue;

const UniValue& UniValue::Object::operator[](const std::string& key) const noexcept
{
    if (auto found = locate(key)) {
        return *found;
    }
    return NullUniValue;
}

const UniValue& UniValue::Object::operator[](size_type index) const noexcept
{
    if (index < vector.size()) {
        return vector[index].second;
    }
    return NullUniValue;
}

const UniValue* UniValue::Object::locate(const std::string& key) const noexcept {
    for (auto& entry : vector) {
        if (entry.first == key) {
            return &entry.second;
        }
    }
    return nullptr;
}
UniValue* UniValue::Object::locate(const std::string& key) noexcept {
    for (auto& entry : vector) {
        if (entry.first == key) {
            return &entry.second;
        }
    }
    return nullptr;
}

const UniValue& UniValue::Object::at(const std::string& key) const {
    if (auto found = locate(key)) {
        return *found;
    }
    throw std::out_of_range("Key not found in JSON object: " + key);
}
UniValue& UniValue::Object::at(const std::string& key) {
    if (auto found = locate(key)) {
        return *found;
    }
    throw std::out_of_range("Key not found in JSON object: " + key);
}

const UniValue& UniValue::Object::at(size_type index) const
{
    if (index < vector.size()) {
        return vector[index].second;
    }
    throw std::out_of_range("Index " + std::to_string(index) + " out of range in JSON object of length " +
                            std::to_string(vector.size()));
}
UniValue& UniValue::Object::at(size_type index)
{
    if (index < vector.size()) {
        return vector[index].second;
    }
    throw std::out_of_range("Index " + std::to_string(index) + " out of range in JSON object of length " +
                            std::to_string(vector.size()));
}

const UniValue& UniValue::Object::front() const noexcept
{
    if (!vector.empty()) {
        return vector.front().second;
    }
    return NullUniValue;
}

const UniValue& UniValue::Object::back() const noexcept
{
    if (!vector.empty()) {
        return vector.back().second;
    }
    return NullUniValue;
}

const UniValue& UniValue::Array::operator[](size_type index) const noexcept
{
    if (index < vector.size()) {
        return vector[index];
    }
    return NullUniValue;
}

const UniValue& UniValue::Array::at(size_type index) const
{
    if (index < vector.size()) {
        return vector[index];
    }
    throw std::out_of_range("Index " + std::to_string(index) + " out of range in JSON array of length " +
                            std::to_string(vector.size()));
}
UniValue& UniValue::Array::at(size_type index)
{
    if (index < vector.size()) {
        return vector[index];
    }
    throw std::out_of_range("Index " + std::to_string(index) + " out of range in JSON array of length " +
                            std::to_string(vector.size()));
}

const UniValue& UniValue::Array::front() const noexcept
{
    if (!vector.empty()) {
        return vector.front();
    }
    return NullUniValue;
}

const UniValue& UniValue::Array::back() const noexcept
{
    if (!vector.empty()) {
        return vector.back();
    }
    return NullUniValue;
}

const std::string UniValue::boolTrueVal{"1"};

void UniValue::setNull() noexcept
{
    typ = VNULL;
    val.clear();
    entries.clear();
    values.clear();
}

void UniValue::setBool(bool val_)
{
    setNull();
    typ = VBOOL;
    if (val_)
        val = boolTrueVal;
}

static bool validNumStr(const std::string& s)
{
    std::string tokenVal;
    unsigned int consumed;
    enum jtokentype tt = getJsonToken(tokenVal, consumed, s.data(), s.data() + s.size());
    return (tt == JTOK_NUMBER);
}

void UniValue::setNumStr(const std::string& val_)
{
    if (!validNumStr(val_))
        return;

    setNull();
    typ = VNUM;
    val = val_;
}
void UniValue::setNumStr(std::string&& val_) noexcept
{
    if (!validNumStr(val_))
        return;

    setNull();
    typ = VNUM;
    val = std::move(val_);
}

template<typename Num>
void UniValue::setIntOrFloat(Num num)
{
    if (std::is_same<Num, double>::value) {
        // ensure not NaN or inf, which are not representable by the JSON Number type
        if (!std::isfinite(num))
            return;
        // For floats and doubles, we can't use snprintf() since the C-locale may be anything,
        // which means the decimal character may be anything. What's more, we can't touch the
        // C-locale since it's a global object and is not thread-safe.
        //
        // So, for doubles we must fall-back to using the (slower) std::ostringstream.
        // See BCHN issue #137.
        std::ostringstream oss;
        oss.imbue(std::locale::classic());
        oss << std::setprecision(16) << num;
        setNull();
        typ = VNUM;
        val = oss.str();
    } else {
        // Longest possible integers are "-9223372036854775808" and "18446744073709551615",
        // both of which require 20 visible characters and 1 terminating null,
        // hence buffer size 21.
        constexpr int bufSize = 21;
        constexpr auto fmt =
                std::is_same<Num, double>::value
                ? "%1.16g" // <-- this branch is never taken, it's just here to allow compilation
                : (std::is_same<Num, int64_t>::value
                   ? "%" PRId64
                   : (std::is_same<Num, uint64_t>::value
                      ? "%" PRIu64
                        // this is here to enforce uint64_t, int64_t or double (if evaluated will fail at compile-time)
                      : throw std::runtime_error("Unexpected type")));
        std::array<char, bufSize> buf;
        int n = std::snprintf(buf.data(), size_t(bufSize), fmt, num); // C++11 snprintf always NUL terminates
        if (n <= 0 || n >= bufSize) // should never happen
            return;
        setNull();
        typ = VNUM;
        val.assign(buf.data(), std::string::size_type(n));
    }
}

void UniValue::setInt(uint64_t val_)
{
    setIntOrFloat(val_);
}

void UniValue::setInt(int64_t val_)
{
    setIntOrFloat(val_);
}

void UniValue::setFloat(double val_)
{
    setIntOrFloat(val_);
}

void UniValue::setStr(const std::string& val_)
{
    setNull();
    typ = VSTR;
    val = val_;
}
void UniValue::setStr(std::string&& val_) noexcept
{
    setNull();
    typ = VSTR;
    val = std::move(val_);
}

void UniValue::setArray() noexcept
{
    setNull();
    typ = VARR;
}
void UniValue::setArray(const Array& array)
{
    setArray();
    values = array;
}
void UniValue::setArray(Array&& array) noexcept
{
    setArray();
    values = std::move(array);
}

void UniValue::setObject() noexcept
{
    setNull();
    typ = VOBJ;
}
void UniValue::setObject(const Object& object)
{
    setObject();
    entries = object;
}
void UniValue::setObject(Object&& object) noexcept
{
    setObject();
    entries = std::move(object);
}

void UniValue::push_back(const UniValue& val_)
{
    if (typ != VARR)
        return;

    values.push_back(val_);
}

void UniValue::push_back(UniValue&& val_)
{
    if (typ != VARR)
        return;

    values.emplace_back(std::move(val_));
}

void UniValue::pushKV(const std::string& key, const UniValue& val_, bool check)
{
    if (typ != VOBJ)
        return;
    if (check) {
        if (auto found = locate(key)) {
            *found = val_;
            return;
        }
    }
    entries.emplace_back(key, val_);
}
void UniValue::pushKV(const std::string& key, UniValue&& val_, bool check)
{
    if (typ != VOBJ)
        return;
    if (check) {
        if (auto found = locate(key)) {
            *found = std::move(val_);
            return;
        }
    }
    entries.emplace_back(key, std::move(val_));
}
void UniValue::pushKV(std::string&& key, const UniValue& val_, bool check)
{
    if (typ != VOBJ)
        return;
    if (check) {
        if (auto found = locate(key)) {
            *found = val_;
            return;
        }
    }
    entries.emplace_back(std::move(key), val_);
}
void UniValue::pushKV(std::string&& key, UniValue&& val_, bool check)
{
    if (typ != VOBJ)
        return;
    if (check) {
        if (auto found = locate(key)) {
            *found = std::move(val_);
            return;
        }
    }
    entries.emplace_back(std::move(key), std::move(val_));
}

const UniValue& UniValue::operator[](const std::string& key) const noexcept
{
    if (auto found = locate(key)) {
        return *found;
    }
    return NullUniValue;
}

const UniValue& UniValue::operator[](size_type index) const noexcept
{
    switch (typ) {
    case VOBJ:
        return entries[index];
    case VARR:
        return values[index];
    default:
        return NullUniValue;
    }
}

const UniValue& UniValue::front() const noexcept
{
    switch (typ) {
    case VOBJ:
        return entries.front();
    case VARR:
        return values.front();
    default:
        return NullUniValue;
    }
}

const UniValue& UniValue::back() const noexcept
{
    switch (typ) {
    case VOBJ:
        return entries.back();
    case VARR:
        return values.back();
    default:
        return NullUniValue;
    }
}

const UniValue* UniValue::locate(const std::string& key) const noexcept {
    return entries.locate(key);
}
UniValue* UniValue::locate(const std::string& key) noexcept {
    return entries.locate(key);
}

const UniValue& UniValue::at(const std::string& key) const {
    if (typ == VOBJ) {
        return entries.at(key);
    }
    throw std::domain_error(std::string("Cannot look up keys in JSON ") + uvTypeName(typ) +
                                        ", expected object with key: " + key);
}
UniValue& UniValue::at(const std::string& key) {
    if (typ == VOBJ) {
        return entries.at(key);
    }
    throw std::domain_error(std::string("Cannot look up keys in JSON ") + uvTypeName(typ) +
                                        ", expected object with key: " + key);
}

const UniValue& UniValue::at(size_type index) const
{
    switch (typ) {
    case VOBJ:
        return entries.at(index);
    case VARR:
        return values.at(index);
    default:
        throw std::domain_error(std::string("Cannot look up indices in JSON ") + uvTypeName(typ) +
                                ", expected array or object larger than " + std::to_string(index) + " elements");
    }
}
UniValue& UniValue::at(size_type index)
{
    switch (typ) {
    case VOBJ:
        return entries.at(index);
    case VARR:
        return values.at(index);
    default:
        throw std::domain_error(std::string("Cannot look up indices in JSON ") + uvTypeName(typ) +
                                ", expected array or object larger than " + std::to_string(index) + " elements");
    }
}

bool UniValue::operator==(const UniValue& other) const noexcept
{
    // Type must be equal.
    if (typ != other.typ)
        return false;
    // Some types have additional requirements for equality.
    switch (typ) {
    case VBOOL:
    case VNUM:
    case VSTR:
        return val == other.val;
    case VARR:
        return values == other.values;
    case VOBJ:
        return entries == other.entries;
    case VNULL:
        break;
    }
    // Returning true is the default behavior, but this is not included as a default statement inside the switch statement,
    // so that the compiler warns if some type is not explicitly listed there.
    return true;
}

void UniValue::reserve(size_type n)
{
    switch (typ) {
    case VOBJ:
        entries.reserve(n);
        break;
    case VARR:
        values.reserve(n);
        break;
    default:
        break;
    }
}

const char *uvTypeName(UniValue::VType t) noexcept
{
    switch (t) {
    case UniValue::VNULL: return "null";
    case UniValue::VBOOL: return "bool";
    case UniValue::VOBJ: return "object";
    case UniValue::VARR: return "array";
    case UniValue::VSTR: return "string";
    case UniValue::VNUM: return "number";
    }

    // not reached
    return nullptr;
}
