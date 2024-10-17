#pragma once

/**
* Defines a class and template specialization, for a variable, needed for use with the GET_PRIVATE hook below
 *
 * @param InClass		The class being accessed (not a string, just the class, i.e. FStackTracker)
 * @param VarName		Name of the variable being accessed (again, not a string)
 * @param VarType		The type of the variable being accessed
 */
#define HSCALE_IMPLEMENT_GET_PRIVATE_VAR(InClass, VarName, VarType) \
struct InClass##VarName##Accessor \
{ \
using Member = VarType InClass::*; \
\
friend Member GetPrivate(InClass##VarName##Accessor); \
}; \
\
template struct AccessPrivate<InClass##VarName##Accessor, &InClass::VarName>;

/**
 * A macro for tidying up accessing of private members, through the above code
 *
 * @param InClass		The class being accessed (not a string, just the class, i.e. FStackTracker)
 * @param InObj			Pointer to an instance of the specified class
 * @param MemberName	Name of the member being accessed (again, not a string)
 * @return				The value of the member 
 */
#define HSCALE_GET_PRIVATE(InClass, InObj, MemberName) (*InObj).*GetPrivate(InClass##MemberName##Accessor())

/**
 * Redundant version of the above, for emphasizing the ability to assign a writable reference which can modify the original value
 */
#define HSCALE_GET_PRIVATE_REF(InClass, InObj, MemberName) GET_PRIVATE(InClass, InObj, MemberName)


/**
 * Defines a class used for hack-accessing protected functions, through the CALL_PROTECTED macro below
 *
 * @param InClass		The class being accessed (not a string, just the class, i.e. FStackTracker)
 * @param FuncName		Name of the function being accessed (again, not a string)
 * @param FuncRet		The return type of the function
 * @param FuncParms		The function parameters
 * @param FuncParmNames	The names of the function parameters, as passed from one function to another
 * @param FuncModifier	(Optional) Modifier placed after the function - usually 'const'
 */
#define HSCALE_IMPLEMENT_GET_PROTECTED_FUNC_CONST(InClass, FuncName, FuncRet, FuncParms, FuncParmNames, FuncModifier) \
struct InClass##FuncName##Accessor : public InClass \
{ \
FORCEINLINE FuncRet FuncName##Accessor(FuncParms) FuncModifier \
{\
return FuncName(FuncParmNames); \
} \
};

#define HSCALE_IMPLEMENT_GET_PROTECTED_FUNC(InClass, FuncName, FuncRet, FuncParms, FuncParmNames) \
IMPLEMENT_GET_PROTECTED_FUNC_CONST(InClass, FuncName, FuncRet, FuncParms, FuncParmNames,)

// Version of GET_PRIVATE, for calling protected functions
#define HSCALE_CALL_PROTECTED(InClass, InObj, MemberName) ((InClass##MemberName##Accessor*)&(*InObj))->MemberName##Accessor

// Hack for accessing private members in various Engine classes

// This template, is used to link an arbitrary class member, to the GetPrivate function
template<typename Accessor, typename Accessor::Member Member> struct AccessPrivate
{
	friend typename Accessor::Member GetPrivate(Accessor InAccessor)
	{
		return Member;
	}
};