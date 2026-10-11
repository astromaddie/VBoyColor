#pragma once

#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

// Just enough JSON to read a web API's answer (the updater's GitHub release
// list): a small parser into a tree of values.
struct JsonValue
{
    enum class Type
    {
        Null,
        Bool,
        Number,
        String,
        Array,
        Object
    };
    Type type = Type::Null;
    bool boolean = false;
    double number = 0.0;
    std::string string;
    std::vector<JsonValue> array;
    std::vector<std::pair<std::string, JsonValue>> object;

    // An object's member, or nullptr.
    const JsonValue *Get(const char *key) const
    {
        for (const auto &member : object)
            if (member.first == key)
                return &member.second;
        return nullptr;
    }
    std::string Str(const char *key) const
    {
        const JsonValue *v = Get(key);
        return v && v->type == Type::String ? v->string : std::string();
    }
    bool Bool(const char *key) const
    {
        const JsonValue *v = Get(key);
        return v && v->type == Type::Bool && v->boolean;
    }
};

namespace MiniJsonDetail
{
    struct Parser
    {
        const std::string &s;
        size_t i = 0;
        int depth = 0;

        void Space()
        {
            while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r'))
                ++i;
        }
        bool Literal(const char *word)
        {
            const size_t n = std::char_traits<char>::length(word);
            if (s.compare(i, n, word) != 0)
                return false;
            i += n;
            return true;
        }
        static void Utf8(std::string &out, unsigned cp)
        {
            if (cp < 0x80)
                out += static_cast<char>(cp);
            else if (cp < 0x800)
            {
                out += static_cast<char>(0xC0 | (cp >> 6));
                out += static_cast<char>(0x80 | (cp & 0x3F));
            }
            else if (cp < 0x10000)
            {
                out += static_cast<char>(0xE0 | (cp >> 12));
                out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                out += static_cast<char>(0x80 | (cp & 0x3F));
            }
            else
            {
                out += static_cast<char>(0xF0 | (cp >> 18));
                out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
                out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                out += static_cast<char>(0x80 | (cp & 0x3F));
            }
        }
        bool Hex4(unsigned &v)
        {
            if (i + 4 > s.size())
                return false;
            v = 0;
            for (int k = 0; k < 4; ++k)
            {
                const char c = s[i++];
                v <<= 4;
                if (c >= '0' && c <= '9')
                    v |= c - '0';
                else if (c >= 'a' && c <= 'f')
                    v |= c - 'a' + 10;
                else if (c >= 'A' && c <= 'F')
                    v |= c - 'A' + 10;
                else
                    return false;
            }
            return true;
        }
        bool String(std::string &out)
        {
            if (i >= s.size() || s[i] != '"')
                return false;
            ++i;
            while (i < s.size())
            {
                const char c = s[i++];
                if (c == '"')
                    return true;
                if (c != '\\')
                {
                    out += c;
                    continue;
                }
                if (i >= s.size())
                    return false;
                const char e = s[i++];
                switch (e)
                {
                case '"': out += '"'; break;
                case '\\': out += '\\'; break;
                case '/': out += '/'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                case 't': out += '\t'; break;
                case 'u':
                {
                    unsigned cp = 0;
                    if (!Hex4(cp))
                        return false;
                    if (cp >= 0xD800 && cp < 0xDC00 && s.compare(i, 2, "\\u") == 0)
                    {
                        i += 2;
                        unsigned low = 0;
                        if (!Hex4(low))
                            return false;
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                    }
                    Utf8(out, cp);
                    break;
                }
                default:
                    return false;
                }
            }
            return false;
        }
        bool Value(JsonValue &v)
        {
            if (++depth > 64)
                return false;
            Space();
            if (i >= s.size())
                return false;
            bool ok = true;
            const char c = s[i];
            if (c == '{')
            {
                v.type = JsonValue::Type::Object;
                ++i;
                Space();
                if (i < s.size() && s[i] == '}')
                    ++i;
                else
                    for (;;)
                    {
                        Space();
                        std::pair<std::string, JsonValue> member;
                        if (!String(member.first))
                            return false;
                        Space();
                        if (i >= s.size() || s[i++] != ':' || !Value(member.second))
                            return false;
                        v.object.push_back(std::move(member));
                        Space();
                        if (i < s.size() && s[i] == ',')
                        {
                            ++i;
                            continue;
                        }
                        if (i < s.size() && s[i] == '}')
                        {
                            ++i;
                            break;
                        }
                        return false;
                    }
            }
            else if (c == '[')
            {
                v.type = JsonValue::Type::Array;
                ++i;
                Space();
                if (i < s.size() && s[i] == ']')
                    ++i;
                else
                    for (;;)
                    {
                        JsonValue item;
                        if (!Value(item))
                            return false;
                        v.array.push_back(std::move(item));
                        Space();
                        if (i < s.size() && s[i] == ',')
                        {
                            ++i;
                            continue;
                        }
                        if (i < s.size() && s[i] == ']')
                        {
                            ++i;
                            break;
                        }
                        return false;
                    }
            }
            else if (c == '"')
            {
                v.type = JsonValue::Type::String;
                ok = String(v.string);
            }
            else if (Literal("true"))
            {
                v.type = JsonValue::Type::Bool;
                v.boolean = true;
            }
            else if (Literal("false"))
                v.type = JsonValue::Type::Bool;
            else if (Literal("null"))
                v.type = JsonValue::Type::Null;
            else
            {
                const char *start = s.c_str() + i;
                char *end = nullptr;
                v.type = JsonValue::Type::Number;
                v.number = std::strtod(start, &end);
                if (end == start)
                    return false;
                i += static_cast<size_t>(end - start);
            }
            --depth;
            return ok;
        }
    };
} // namespace MiniJsonDetail

// Parses text into out - false if it isn't JSON.
inline bool ParseJson(const std::string &text, JsonValue &out)
{
    MiniJsonDetail::Parser parser{text};
    out = JsonValue{};
    return parser.Value(out);
}
