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

function void *
GetMemberPointer(void *data, const MemberInfo *info)
{
    return (char *)data + info->offset;
}

#endif /* TEXTIT_INTROSPECTION_MACROS_HPP */
