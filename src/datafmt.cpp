/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */
#include <iomanip>

#include "dataimpl.h"

#include <epicsTime.h>
#include <alarm.h>
#include <alarmString.h>
namespace pvxs {

namespace {

struct FmtDelta {
    std::ostream& strm;
    const Value::Fmt& fmt;

    void field(const std::string& prefix, const Value& val, bool verytop)
    {
        if(verytop && !val.isMarked(false))
            return;

        strm<<indent{}<<prefix;
        if(!verytop)
            strm<<" ";
        strm<<val.type().name();
        if(val.type()==TypeCode::Struct && !val.id().empty()) {
            strm<<" \""<<escape(val.id())<<'"';
        }

        if(fmt._showValue) {
            auto store = Value::Helper::store_ptr(val);

            switch(val.storageType()) {
            case StoreType::Real:     strm<<" = "<<store->as<double>(); break;
            case StoreType::Integer:  strm<<" = "<<store->as<int64_t>(); break;
            case StoreType::UInteger: strm<<" = "<<store->as<uint64_t>(); break;
            case StoreType::Bool:     strm<<" = "<<(store->as<bool>() ? "true" : "false"); break;
            case StoreType::String:   strm<<" = \""<<escape(store->as<std::string>())<<"\""; break;
            case StoreType::Array: {
                auto& varr = store->as<shared_array<const void>>();
                if(varr.original_type()!=ArrayType::Value) {
                    strm<<" = "<<varr.format().limit(fmt._limit);
                }
            }
                break;
            default:
                break;
            }
        }

        strm<<"\n";

        switch(val.type().code) {
        case TypeCode::Union:
        case TypeCode::Any: {
            auto uval = val.as<Value>();
            std::string cprefix(prefix);
            cprefix+="->";

            if(val.type()==TypeCode::Union) {
                auto desc = Value::Helper::desc(val);
                auto udesc = Value::Helper::desc(uval);
                for(auto idx : range(desc->members.size())) {
                    if(udesc == &desc->members[idx]) {
                        cprefix+=desc->miter[idx].first;
                        break;
                    }
                }
            }

            top(cprefix, uval, false);
        }
            break;
        case TypeCode::StructA:
        case TypeCode::UnionA:
        case TypeCode::AnyA: {
            auto rawval = val.as<const shared_array<const void>>();
            if(rawval.original_type()==ArrayType::Null) {

            } else if(rawval.original_type()==ArrayType::Value) {
                auto aval = rawval.castTo<const Value>();

                for(auto idx : range(aval.size())) {
                    std::ostringstream strm;
                    strm<<indent{}<<prefix<<'['<<idx<<']';

                    top(strm.str(), aval[idx], false);
                }

            } else {
                throw std::logic_error("Value[] is not");
            }
        }
            break;
        default:
            break;
        }
    }

    void top(const std::string& prefix, const Value& val, bool verytop)
    {
        if(!val) {
            strm<<indent{}<<prefix;
            if(!verytop)
                strm<<' ';
            strm<<"null\n";
            return;
        }

        field(prefix, val, verytop);

        if(val.type()==TypeCode::Struct) {
            for(const auto& fld : val.imarked()) {
                std::string cprefix(prefix);
                if(!verytop)
                    cprefix += '.';
                cprefix += val.nameOf(fld);
                field(cprefix, fld, false);
            }
        }
    }
};

struct FmtTree {
    std::ostream& strm;
    const Value::Fmt& fmt;

    void show_value(const Value& fld) {
        const auto type(fld.type());
        assert(type.kind()!=Kind::Compound);

        switch(type.code) {
        case TypeCode::Bool:
            strm<<(fld.as<bool>() ? "true" : "false");
            return;
        case TypeCode::Int8:
        case TypeCode::Int16:
        case TypeCode::Int32:
        case TypeCode::Int64:
            strm<<fld.as<int64_t>();
            return;
        case TypeCode::UInt8:
        case TypeCode::UInt16:
        case TypeCode::UInt32:
        case TypeCode::UInt64:
            strm<<fld.as<uint64_t>();
            return;
        case TypeCode::Float32:
        case TypeCode::Float64:
            strm<<fld.as<double>();
            return;
        case TypeCode::String:
            strm<<"\""<<escape(fld.as<std::string>())<<"\"";
            return;
        case TypeCode::BoolA:
        case TypeCode::Int8A:
        case TypeCode::Int16A:
        case TypeCode::Int32A:
        case TypeCode::Int64A:
        case TypeCode::UInt8A:
        case TypeCode::UInt16A:
        case TypeCode::UInt32A:
        case TypeCode::UInt64A:
        case TypeCode::Float32A:
        case TypeCode::Float64A:
        case TypeCode::StringA:
        {
            auto varr = fld.as<shared_array<const void>>();
            strm<<varr.format().limit(fmt._limit);
        }
            return;
        case TypeCode::Any:
        case TypeCode::Struct:
        case TypeCode::Union:
        case TypeCode::StructA:
        case TypeCode::UnionA:
        case TypeCode::AnyA:
            assert(false);
            break;
        default:
            strm<<"!!Invalid TypeCode!! "<<int(type.code)<<"\n";
            return;
        }
    }

    // each invocation emits at least one complete line
    void show(const Value& fld, const std::string& member) {
        // caller should indent{}
        if(!fld) {
            strm<<"null\n";
            return;
        }

        const auto type(fld.type());

        strm<<type;
        {
            auto id(fld.id());
            if(!id.empty())
                strm<<" \""<<id<<"\"";
        }

        if(type.kind()!=Kind::Compound) {
            if(!member.empty())
                strm<<' '<<member;
            if(fmt._showValue) {
                strm<<" = ";
                show_value(fld);
            }
            strm<<"\n";
            return;
        }

        if(type==TypeCode::Any
                || (!fmt._showValue && type==TypeCode::AnyA)
                || (fmt._showValue && type==TypeCode::Union)) {
            // any NAME = VAL
            // union NAME.MEM TYPE = VAL

            Value val;
            if(fmt._showValue)
                val = fld.as<Value>();

            if(!member.empty())
                strm<<' '<<member;

            if(type==TypeCode::Union && val) { // implied _showValue
                auto mem(fld.nameOf(val));
                strm<<'.'<<mem;
            }
            if(fmt._showValue) {
                strm<<" ";
                show(val, std::string());
            } else {
                strm<<"\n";
            }
            return;

        } else if(type==TypeCode::Struct
                  || type==TypeCode::Union // && !_showValue
                  || (!fmt._showValue && type==TypeCode::StructA)
                  || (!fmt._showValue && type==TypeCode::UnionA))
        {
            // struct "id" { ... } NAME

            Value def;
            if(!type.isarray()) {
                def = fld;
            } else { // StructA, UnionA
                //def = fld.allocMember(); // can't call directly due to const
                auto desc(Value::Helper::desc(fld));
                auto store(Value::Helper::store(fld));
                decltype (store->top->desc) fld(store->top->desc, desc->members.data());
                def = Value::Helper::build(fld); // not connection to fld (not parent)
            }

            strm<<" {";
            bool first = true;
            {
                Indented I(strm);
                for(auto mem : def.ichildren()) {
                    auto mname(def.nameOf(mem));
                    if(first)
                        strm<<'\n';
                    strm<<indent{};
                    show(mem, mname);
                    first = false;
                }
            }
            if(!first)
                strm<<indent{};
            strm<<'}';

            if(!member.empty())
                strm<<' '<<member;
            strm<<"\n";

        } else {
            // struct[] NAME = [ ... ]

            if(!member.empty())
                strm<<' '<<member;

            auto arr(fld.as<shared_array<const Value>>());
            strm<<" = {"<<arr.size()<<"}[";
            size_t shown = 0u;
            {
                Indented I(strm);

                for(auto& elem : arr) {
                    if(!shown)
                        strm<<'\n';
                    strm<<indent{};
                    if(fmt._limit && shown>=fmt._limit) {
                        strm<<"...\n";
                        break;
                    }
                    show(elem, std::string());
                    shown++;
                }
            }

            if(shown)
                strm<<indent{};
            strm<<"]\n";
        }
    }
};

constexpr int NUM_PVA_ALARM_STATUSES = 8;
static constexpr const char* pvaAlarmStatusStrings[NUM_PVA_ALARM_STATUSES] = {
    "NO_STATUS",
    "DEVICE",
    "DRIVER",
    "RECORD",
    "DB",
    "CONF",
    "UNDEFINED",
    "CLIENT",
};

/** 
 * \brief Display name+value only, not the full structure
 */
struct FmtValue {
    std::ostream& stream;
    const Value::Fmt& fmt;
    
    /**
     * \brief Display timestamp, or <undefined> if none.
     */
    void show_timestamp(const Value& field) {
        epicsTimeStamp ts = {};
        timespec tv = {};
        tm tm = {};

        auto secsField = field["timeStamp.secondsPastEpoch"];
        if (secsField.valid()) {
            tv.tv_sec = secsField.as<int64_t>();
        } else {
            stream << "<undefined> ";
            return; // Nothing to show
        }

        auto nsecsField = field["timeStamp.nanoseconds"];
        if (nsecsField) {
            tv.tv_nsec = nsecsField.as<int32_t>();
        }

        epicsTimeFromTimespec(&ts, &tv);

        if (localtime_r(&tv.tv_sec, &tm)) {
            char buf[128];
            strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
            // Print the fractional part of the second.
            stream << buf << "." 
                << std::setfill('0') << std::setw(3) << uint64_t(tv.tv_nsec / 1e6)
                << " ";
        } else {
            stream << "<unknown> ";
        }
    }
    
    /**
     * \brief Show alarms if status != NO_ALARM
     * Will be in the format " SEVERITY COND [MSG ]", or "" if no alarm is present.
     */
    void maybe_show_alarms(const Value& field) {
        int alarmCond = 0;
        auto alarmFld = field.lookup("alarm.status");
        if (alarmFld && (alarmCond = alarmFld.as<int32_t>()) == 0)
            return;
        alarmCond = clamp(alarmCond, 0, NUM_PVA_ALARM_STATUSES);

        std::string msg;
        auto msgFld = field.lookup("alarm.message");
        if (msgFld && msgFld.type() == TypeCode::String)
            msg = msgFld.as<std::string>();

        int sevr = epicsSevNone;
        auto alarmSevr = field.lookup("alarm.severity");
        if (alarmSevr)
            sevr = clamp(alarmSevr.as<int>(), firstEpicsAlarmSev, lastEpicsAlarmSev);

        stream << " " << epicsAlarmSeverityStrings[sevr]
               << " " << pvaAlarmStatusStrings[alarmCond] << " ";

        if (!msg.empty())
            stream << msg << " ";
    }

    /**
     * \brief Show a simple scalar or array value
     */
    void show_value(const Value& field, std::ostream* mystream = nullptr) {
        std::ostream& stream = mystream ? *mystream : this->stream;

        switch(field.type().code) {
        case TypeCode::Bool:
            stream << field.as<bool>();
            break;
        case TypeCode::Int8:
        case TypeCode::Int16:
        case TypeCode::Int32:
        case TypeCode::Int64:
            stream << field.as<int64_t>();
            break;
        case TypeCode::UInt8:
        case TypeCode::UInt16:
        case TypeCode::UInt32:
        case TypeCode::UInt64:
            stream << field.as<uint64_t>();
            break;
        case TypeCode::Float32:
        case TypeCode::Float64:
            stream << field.as<double>();
            break;
        case TypeCode::String:
            stream << "\"" << field.as<std::string>() << "\"";
            break;
        case TypeCode::BoolA:
        case TypeCode::Int8A:
        case TypeCode::Int16A:
        case TypeCode::Int32A:
        case TypeCode::Int64A:
        case TypeCode::UInt8A:
        case TypeCode::UInt16A:
        case TypeCode::UInt32A:
        case TypeCode::UInt64A:
        case TypeCode::Float32A:
        case TypeCode::Float64A:
        case TypeCode::StringA:
            stream << field.as<shared_array<const void>>().format()
                .limit(fmt._limit);
            break;
        case TypeCode::Null:
            break;
        case TypeCode::Struct:
        case TypeCode::Union:
        case TypeCode::Any:
        case TypeCode::StructA:
        case TypeCode::UnionA:
        case TypeCode::AnyA:
            break; // TODO!!
        }
    }

    template<typename T>
    T cast(const shared_array<const void>& arr, size_t i) {
        return reinterpret_cast<const T*>(arr.data())[i];
    }

    void show_array_value(const shared_array<const void>& arr, size_t i, std::ostream* mystream = nullptr) {
        std::ostream& stream = mystream ? *mystream : this->stream;

        switch (arr.original_type()) {
        case ArrayType::Null:
            break;
        case ArrayType::Bool:
            stream << cast<bool>(arr, i); break;
        case ArrayType::Int8:
            stream << cast<int8_t>(arr, i); break;
        case ArrayType::Int16:
            stream << cast<int16_t>(arr, i); break;
        case ArrayType::Int32:
            stream << cast<int32_t>(arr, i); break;
        case ArrayType::Int64:
            stream << cast<int64_t>(arr, i); break;
        case ArrayType::UInt8:
            stream << cast<uint8_t>(arr, i); break;
        case ArrayType::UInt16:
            stream << cast<uint16_t>(arr, i); break;
        case ArrayType::UInt32:
            stream << cast<uint32_t>(arr, i); break;
        case ArrayType::UInt64:
            stream << cast<uint64_t>(arr, i); break;
        case ArrayType::Float32:
            stream << cast<float>(arr, i); break;
        case ArrayType::Float64:
            stream << cast<double>(arr, i); break;
        case ArrayType::String:
            stream << cast<std::string>(arr, i); break;
        case ArrayType::Value:
            show_value(arr.castTo<const Value>().at(i));
            break;
        }
    }

    /**
     * \brief Show an enum value (string)
     */
    void show_enum_value(const Value& field) {
        auto indexField = field["value.index"];
        if (!indexField.valid() || indexField.type().kind() != Kind::Integer)
            return;
        auto index = indexField.as<int32_t>();

        auto choices = field["value.choices"];
        if (!choices.valid() || choices.type() != TypeCode::StringA) {
            stream << index;
            return;
        }

        auto array = choices.as<shared_array<const void>>();
        if (index < 0 || size_t(index) >= array.size()) {
            stream << "<invalid choice>";
            return;
        }
        stream << "(" << index << ") " 
            << array.castTo<const std::string>().at(index);
    }

    /**
     * \brief Display a table with labels as column headers.
     * This prints the table in a human readable manner and is much less efficient
     * than a non-pretty printer for i.e. CSV formatted prints.
     */
    void show_table_human(const Value& field) {
        auto labelsField = field["labels"];
        if (!labelsField.valid())
            return;
        auto labels = labelsField.as<shared_array<const void>>()
                                 .castTo<const std::string>();

        auto value = field["value"];

        std::vector<shared_array<const void>> valueCols;
        valueCols.reserve(labels.size());

        std::vector<size_t> colWidths;
        colWidths.reserve(labels.size());

        size_t nrows = 0;

        // Collect the label widths and grab columns of values.
        for (const auto& label : labels) {
            colWidths.push_back(label.size()+1);

            if (value.valid()) {
                valueCols.push_back(value[label].as<shared_array<const void>>());
                nrows = std::max(nrows, valueCols.back().size());
            }
        }

        // TODO: This kind of sucks, figure out a better algorithm.
        std::vector<std::stringstream> converted;
        converted.resize(nrows * valueCols.size());

        // Pass 1: Bulk conversion of values to strings
        for (size_t row = 0; row < nrows; ++row) {
            for (size_t col = 0; col < valueCols.size(); ++col) {
                const auto& valueCol = valueCols[col];
                if (row < valueCol.size()) {
                    std::ostream& mystream = converted[row * valueCols.size() + col];
                    show_array_value(valueCol, row, &mystream);

                    // Account for additional column spacing for column 1 and onwards.
                    size_t rowWidth = size_t(mystream.tellp()) + (col == 0U ? 0 : 1);

                    colWidths[col] = std::max(rowWidth, colWidths[col]);
                }
            }
        }

        // Print the labels now (with widths)
        for (size_t col = 0; col < labels.size(); col++) {
            stream << std::setw(colWidths[col]) << std::right;
            stream << labels[col];
        }
        stream << "\n";

        // "Pretty" printing, with the widths we calculated.
        for (size_t row = 0; row < nrows; ++row) {
            for (size_t col = 0; col < valueCols.size(); ++col) {
                const auto& valueCol = valueCols[col];
                stream << std::setw(colWidths[col]) << std::right;

                if (row < valueCol.size()) {
                    show_array_value(valueCol, row);
                } else {
                    // if we've run out of values in this column, print spaces.
                    stream << " ";
                }
            }

            // We add a newline later for the final row.
            if (row+1 != nrows)
                stream << "\n";
        }
    }

    /**
     * \brief Shows the EGU of the field, if present.
     */
    void show_egu(const Value& field) {
        auto egu = field.lookup("display.units");
        if (!egu)
            return;

        auto msg = egu.as<std::string>();
        if (!msg.empty())
            stream << " " << msg << " ";
    }
    
    /**
     * \brief Shows a PV's value field and some other relevant fields.
     * Displayed format is dependent on the underlying NT type.
     *
     * NTScalar:        <TIME> [NAME] <VALUE> [EGU] [ALARM]
     *
     * NTScalarArray:   <TIME> [NAME] [ALARM] <VALUE ...>
     *
     * NTTable:         <TIME> [NAME] [ALARM]
     *                    <ROW1 ...>
     *                    ...
     *                    <ROWN ...>

     */
    void show(const std::string& name, const Value& field) {
        const bool isScalar = field.idStartsWith("epics:nt/NTScalar");
        const bool isEnum = field.idStartsWith("epics:nt/NTEnum");
        const bool isArray = field.idStartsWith("epics:nt/NTScalarArray");
        const bool isTable = field.idStartsWith("epics:nt/NTTable");

        // Anything other than scalars is unsupported for now.
        if (!isScalar && !isEnum && !isArray && !isTable)
            return;

        auto val = field.lookup("value");
        if (!val)
            return;

        // Terse mode shows only value and returns.
        if (fmt._terseMode) {
            show_value(val);
            stream << "\n";
            return;
        }

        show_timestamp(field);
        stream << name << " ";

        if (isScalar) {
            // VALUE [EGU] [ALARM]
            show_value(val);
            show_egu(field);
            maybe_show_alarms(field);
        } else if (isEnum) {
            show_enum_value(field);
            maybe_show_alarms(field);
            show_value(val);
        } else if (isArray) {
            // [ALARM] VALUES...
            maybe_show_alarms(field);
            show_value(field);
        } else if (isTable) {
            maybe_show_alarms(field);
            stream << "\n";
            show_table_human(field);
        }

        stream << "\n";
    }
};

} // namespace

std::ostream& operator<<(std::ostream& strm, const Value::Fmt& fmt)
{
    switch (fmt._format) {
    case Value::Fmt::Tree:
        strm<<indent{};
        FmtTree{strm, fmt}.show(*fmt.top, std::string());
        break;
    case Value::Fmt::Delta:
        FmtDelta{strm, fmt}.top("", *fmt.top, true);
        break;
    case Value::Fmt::Val:
        FmtValue{strm, fmt}.show(fmt._name, *fmt.top);
        break;
    default:
        strm<<"<Unknown Value format()>\n";
    }
    return strm;
}

}
