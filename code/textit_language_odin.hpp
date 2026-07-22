#ifndef TEXTIT_LANGUAGE_ODIN_HPP
#define TEXTIT_LANGUAGE_ODIN_HPP

enum TokeKind_Odin : TokenSubKind
{
	Token_Odin_None,
	Token_Odin_Arrow,
	Token_Odin_ConstDeclaration,
	Token_Odin_Declaration,
	Token_Odin_Uninitialized,
};

enum TagKind_Odin : TagSubKind
{
    Tag_Odin_None,

    Tag_Odin_Struct,
    Tag_Odin_Union,
    Tag_Odin_Enum,
    Tag_Odin_EnumValue,
    Tag_Odin_Proc,
    Tag_Odin_Constant,
	Tag_Odin_Global,
};

#endif
