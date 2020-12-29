#ifndef CORE_TYPE_HPP
#define CORE_TYPE_HPP

#include <string>
#include <utility>
#include <variant>

namespace CYX
{

    class Value
    {
      public:
        Value() = default;
        //
        template<typename T>
        explicit Value(T value) : _value(value){};
        //
        template<typename T>
        bool is()
        {
            return std::holds_alternative<T>(_value);
        }
        //
        template<typename T>
        T value()
        {
            return std::get<T>(_value);
        }
        template<typename T>
        T *valuePtr()
        {
            return std::get_if<T>(&_value);
        }
        bool isSameType(const Value &that)
        {
            return _value.index() == that._value.index();
        }
        // type conversion.
        template<typename T>
        T as()
        {
            if (is<T>()) return value<T>();
            if constexpr (std::is_same<T, std::string>::value) return asString();
            if constexpr (std::is_same<T, int>::value) return asInt();
            if constexpr (std::is_same<T, double>::value) return asDouble();
        }
        bool hasValue()
        {
            return _value.index() != 0;
        }

      private:
        std::string asString()
        {
            if (is<int>()) return std::to_string(value<int>());
            if (is<double>()) return std::to_string(value<double>());
        }

        int asInt()
        {
            if (is<double>()) return static_cast<int>(value<double>());
            // TODO std::string should not reach here
        }
        double asDouble()
        {
            if (is<int>()) return static_cast<double>(value<int>());
            // TODO std::string should not reach here
        }

      private:
        std::variant<std::monostate, int, double, std::string> _value;
    };

} // namespace CYX

#endif
