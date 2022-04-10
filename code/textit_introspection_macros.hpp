#ifndef TEXTIT_INTROSPECTION_MACROS_HPP
#define TEXTIT_INTROSPECTION_MACROS_HPP

struct MemberInfo
{
    String name;
    StringID type;
    size_t size;
    size_t offset;
};

template <typename T>
struct Introspection;

#define IntrospectedStructMember(_, Type, Name) Type Name;
#define IntrospectedStructTypeTableEntry(StructName, Type, Name) { Paste(Stringize(Name), _str), Paste(Stringize(Type), _id), sizeof(Type), offsetof(StructName, Name) },
#define DeclareIntrospectedStruct(X)                                                           \
    struct X { X(_, IntrospectedStructMember) };                                               \
    template <> struct Introspection<X>                                                        \
    {                                                                                          \
        static const inline MemberInfo members[] = { X(X, IntrospectedStructTypeTableEntry) }; \
        static const inline size_t member_count = ArrayCount(members);                         \
    };

template <typename T>
function const MemberInfo *
FindMember(String name)
{
    const MemberInfo *result = nullptr;

    for (const MemberInfo &member : Introspection<T>::members)
    {
        if (MatchPrefix(member.name, name))
        {
            result = &member;
            break;
        }
    }

    return result;
}

function void *
GetMemberPointer(void *data, const MemberInfo *info)
{
    return (char *)data + info->offset;
}

template <typename T> 
function void
ReadMemberAs(void *data, const MemberInfo *member, T *out)
{
    *out = *(T *)GetMemberPointer(data, member);
}

#endif /* TEXTIT_INTROSPECTION_MACROS_HPP */
