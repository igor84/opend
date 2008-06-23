#include "gen/llvm.h"

#include "mtype.h"
#include "dsymbol.h"
#include "aggregate.h"
#include "declaration.h"
#include "init.h"

#include "gen/irstate.h"
#include "gen/tollvm.h"
#include "gen/llvmhelpers.h"
#include "gen/arrays.h"
#include "gen/runtime.h"
#include "gen/logger.h"
#include "gen/dvalue.h"

//////////////////////////////////////////////////////////////////////////////////////////

const LLStructType* DtoArrayType(Type* arrayTy)
{
    assert(arrayTy->next);
    const LLType* elemty = DtoType(arrayTy->next);
    if (elemty == LLType::VoidTy)
        elemty = LLType::Int8Ty;
    return LLStructType::get(DtoSize_t(), getPtrToType(elemty), 0);
}

const LLStructType* DtoArrayType(const LLType* t)
{
    return LLStructType::get(DtoSize_t(), getPtrToType(t), 0);
}

//////////////////////////////////////////////////////////////////////////////////////////

const LLArrayType* DtoStaticArrayType(Type* t)
{
    if (t->ir.type)
        return isaArray(t->ir.type->get());

    assert(t->ty == Tsarray);
    assert(t->next);

    const LLType* at = DtoType(t->next);

    TypeSArray* tsa = (TypeSArray*)t;
    assert(tsa->dim->type->isintegral());
    const LLArrayType* arrty = LLArrayType::get(at,tsa->dim->toUInteger());

    assert(!tsa->ir.type);
    tsa->ir.type = new LLPATypeHolder(arrty);
    return arrty;
}

//////////////////////////////////////////////////////////////////////////////////////////

void DtoSetArrayToNull(LLValue* v)
{
    Logger::println("DtoSetArrayToNull");
    LOG_SCOPE;

    LLValue* len = DtoGEPi(v,0,0);
    LLValue* zerolen = llvm::ConstantInt::get(len->getType()->getContainedType(0), 0, false);
    DtoStore(zerolen, len);

    LLValue* ptr = DtoGEPi(v,0,1);
    const LLPointerType* pty = isaPointer(ptr->getType()->getContainedType(0));
    LLValue* nullptr = llvm::ConstantPointerNull::get(pty);
    DtoStore(nullptr, ptr);
}

//////////////////////////////////////////////////////////////////////////////////////////

void DtoArrayAssign(LLValue* dst, LLValue* src)
{
    Logger::println("DtoArrayAssign");
    LOG_SCOPE;

    assert(gIR);
    if (dst->getType() == src->getType())
    {
        LLValue* ptr = DtoGEPi(src,0,0);
        LLValue* val = DtoLoad(ptr);
        ptr = DtoGEPi(dst,0,0);
        DtoStore(val, ptr);

        ptr = DtoGEPi(src,0,1);
        val = DtoLoad(ptr);
        ptr = DtoGEPi(dst,0,1);
        DtoStore(val, ptr);
    }
    else
    {
        Logger::cout() << "array assignment type dont match: " << *dst->getType() << "\n\n" << *src->getType() << '\n';
        const LLArrayType* arrty = isaArray(src->getType()->getContainedType(0));
        if (!arrty)
        {
            Logger::cout() << "invalid: " << *src << '\n';
            assert(0);
        }
        const LLType* dstty = getPtrToType(arrty->getElementType());

        LLValue* dstlen = DtoGEPi(dst,0,0);
        LLValue* srclen = DtoConstSize_t(arrty->getNumElements());
        DtoStore(srclen, dstlen);

        LLValue* dstptr = DtoGEPi(dst,0,1);
        LLValue* srcptr = DtoBitCast(src, dstty);
        DtoStore(srcptr, dstptr);
    }
}

//////////////////////////////////////////////////////////////////////////////////////////

typedef const LLType* constLLVMTypeP;

static size_t checkRectArrayInit(const LLType* pt, constLLVMTypeP& finalty)
{
    if (const LLArrayType* arrty = isaArray(pt)) {
        size_t n = checkRectArrayInit(arrty->getElementType(), finalty);
        size_t ne = arrty->getNumElements();
        if (n) return n * ne;
        return ne;
    }
    finalty = pt;
    return 0;
}

void DtoArrayInit(DValue* array, DValue* value)
{
    Logger::println("DtoArrayInit");
    LOG_SCOPE;

    LLValue* dim = DtoArrayLen(array);
    LLValue* ptr = DtoArrayPtr(array);
    LLValue* val = value->getRVal();

    Logger::cout() << "llvm values:\n" << " ptr: " << *ptr << " dim: " << *dim << " val: " << *val << '\n';

    const LLType* pt = ptr->getType()->getContainedType(0);
    const LLType* t = val->getType();
    const LLType* finalTy;

    size_t aggrsz = 0;
    Type* valtype = value->getType()->toBasetype();

    // handle rectangular init
    if (size_t arrsz = checkRectArrayInit(pt, finalTy))
    {
        assert(finalTy == t);
        LLConstant* c = isaConstant(dim);
        assert(c);
        dim = llvm::ConstantExpr::getMul(c, DtoConstSize_t(arrsz));
        ptr = gIR->ir->CreateBitCast(ptr, getPtrToType(finalTy), "tmp");
    }
    // handle null aggregate
    else if (isaStruct(t))
    {
        aggrsz = getABITypeSize(t);
        LLConstant* c = isaConstant(val);
        assert(c && c->isNullValue());
        LLValue* nbytes;
        if (aggrsz == 1)
            nbytes = dim;
        else
            nbytes = gIR->ir->CreateMul(dim, DtoConstSize_t(aggrsz), "tmp");
        DtoMemSetZero(ptr,nbytes);
        return;
    }
    // handle general aggregate case
    else if (DtoIsPassedByRef(valtype))
    {
        aggrsz = getABITypeSize(pt);
        ptr = gIR->ir->CreateBitCast(ptr, getVoidPtrType(), "tmp");
        val = gIR->ir->CreateBitCast(val, getVoidPtrType(), "tmp");
    }
    else
    {
        Logger::cout() << "no special handling" << '\n';
        assert(t == pt);
    }

    Logger::cout() << "array: " << *ptr << " dim: " << *dim << " val: " << *val << '\n';

    LLSmallVector<LLValue*, 4> args;
    args.push_back(ptr);
    args.push_back(dim);
    args.push_back(val);

    const char* funcname = NULL;

    if (aggrsz) {
        funcname = "_d_array_init_mem";
        args.push_back(DtoConstSize_t(aggrsz));
    }
    else if (isaPointer(t)) {
        funcname = "_d_array_init_pointer";

        const LLType* dstty = getPtrToType(getPtrToType(LLType::Int8Ty));
        if (args[0]->getType() != dstty)
            args[0] = DtoBitCast(args[0],dstty);

        const LLType* valty = getPtrToType(LLType::Int8Ty);
        if (args[2]->getType() != valty)
            args[2] = DtoBitCast(args[2],valty);
    }
    else if (t == LLType::Int1Ty) {
        funcname = "_d_array_init_i1";
    }
    else if (t == LLType::Int8Ty) {
        funcname = "_d_array_init_i8";
    }
    else if (t == LLType::Int16Ty) {
        funcname = "_d_array_init_i16";
    }
    else if (t == LLType::Int32Ty) {
        funcname = "_d_array_init_i32";
    }
    else if (t == LLType::Int64Ty) {
        funcname = "_d_array_init_i64";
    }
    else if (t == LLType::FloatTy) {
        funcname = "_d_array_init_float";
    }
    else if (t == LLType::DoubleTy) {
        funcname = "_d_array_init_double";
    }
    else {
        Logger::cout() << *ptr->getType() << " = " << *val->getType() << '\n';
        assert(0);
    }

    LLFunction* fn = LLVM_D_GetRuntimeFunction(gIR->module, funcname);
    assert(fn);
    Logger::cout() << "calling array init function: " << *fn <<'\n';
    llvm::CallInst* call = llvm::CallInst::Create(fn, args.begin(), args.end(), "", gIR->scopebb());
    call->setCallingConv(llvm::CallingConv::C);
}

//////////////////////////////////////////////////////////////////////////////////////////

void DtoSetArray(LLValue* arr, LLValue* dim, LLValue* ptr)
{
    Logger::println("SetArray");
    DtoStore(dim, DtoGEPi(arr,0,0));
    DtoStore(ptr, DtoGEPi(arr,0,1));
}

//////////////////////////////////////////////////////////////////////////////////////////
LLConstant* DtoConstArrayInitializer(ArrayInitializer* arrinit)
{
    Logger::println("DtoConstArrayInitializer: %s | %s", arrinit->toChars(), arrinit->type->toChars());
    LOG_SCOPE;

    Type* arrinittype = DtoDType(arrinit->type);

    Type* t;
    integer_t tdim;
    if (arrinittype->ty == Tsarray) {
        Logger::println("static array");
        TypeSArray* tsa = (TypeSArray*)arrinittype;
        tdim = tsa->dim->toInteger();
        t = tsa;
    }
    else if (arrinittype->ty == Tarray) {
        Logger::println("dynamic array");
        t = arrinittype;
        tdim = arrinit->dim;
    }
    else
    assert(0);

    Logger::println("dim = %u", tdim);

    std::vector<LLConstant*> inits(tdim, NULL);

    Type* arrnext = arrinittype->next;
    const LLType* elemty = DtoType(arrinittype->next);

    assert(arrinit->index.dim == arrinit->value.dim);
    for (unsigned i=0,j=0; i < tdim; ++i)
    {
        Initializer* init = 0;
        Expression* idx;

        if (j < arrinit->index.dim)
            idx = (Expression*)arrinit->index.data[j];
        else
            idx = NULL;

        LLConstant* v = NULL;

        if (idx)
        {
            Logger::println("%d has idx", i);
            // this is pretty weird :/ idx->type turned out NULL for the initializer:
            //     const in6_addr IN6ADDR_ANY = { s6_addr8: [0] };
            // in std.c.linux.socket
            if (idx->type) {
                Logger::println("has idx->type", i);
                //integer_t k = idx->toInteger();
                //Logger::println("getting value for exp: %s | %s", idx->toChars(), arrnext->toChars());
                LLConstant* cc = idx->toConstElem(gIR);
                Logger::println("value gotten");
                assert(cc != NULL);
                LLConstantInt* ci = llvm::dyn_cast<LLConstantInt>(cc);
                assert(ci != NULL);
                uint64_t k = ci->getZExtValue();
                if (i == k)
                {
                    init = (Initializer*)arrinit->value.data[j];
                    assert(init);
                    ++j;
                }
            }
        }
        else
        {
            if (j < arrinit->value.dim) {
                init = (Initializer*)arrinit->value.data[j];
                ++j;
            }
            else
                v = arrnext->defaultInit()->toConstElem(gIR);
        }

        if (!v)
            v = DtoConstInitializer(t->next, init);
        assert(v);

        inits[i] = v;
        Logger::cout() << "llval: " << *v << '\n';
    }

    Logger::println("building constant array");
    const LLArrayType* arrty = LLArrayType::get(elemty,tdim);
    LLConstant* constarr = LLConstantArray::get(arrty, inits);

    if (arrinittype->ty == Tsarray)
        return constarr;
    else
        assert(arrinittype->ty == Tarray);

    LLGlobalVariable* gvar = new LLGlobalVariable(arrty,true,LLGlobalValue::InternalLinkage,constarr,"constarray",gIR->module);
    LLConstant* idxs[2] = { DtoConstUint(0), DtoConstUint(0) };
    LLConstant* gep = llvm::ConstantExpr::getGetElementPtr(gvar,idxs,2);
    return DtoConstSlice(DtoConstSize_t(tdim),gep);
}

//////////////////////////////////////////////////////////////////////////////////////////
static LLValue* get_slice_ptr(DSliceValue* e, LLValue*& sz)
{
    const LLType* t = e->ptr->getType()->getContainedType(0);
    LLValue* ret = 0;
    if (e->len != 0) {
        // this means it's a real slice
        ret = e->ptr;

        size_t elembsz = getABITypeSize(ret->getType()->getContainedType(0));
        llvm::ConstantInt* elemsz = llvm::ConstantInt::get(DtoSize_t(), elembsz, false);

        if (isaConstantInt(e->len)) {
            sz = llvm::ConstantExpr::getMul(elemsz, isaConstant(e->len));
        }
        else {
            sz = llvm::BinaryOperator::createMul(elemsz,e->len,"tmp",gIR->scopebb());
        }
    }
    else if (isaArray(t)) {
        ret = DtoGEPi(e->ptr, 0, 0);

        size_t elembsz = getABITypeSize(ret->getType()->getContainedType(0));
        llvm::ConstantInt* elemsz = llvm::ConstantInt::get(DtoSize_t(), elembsz, false);

        size_t numelements = isaArray(t)->getNumElements();
        llvm::ConstantInt* nelems = llvm::ConstantInt::get(DtoSize_t(), numelements, false);

        sz = llvm::ConstantExpr::getMul(elemsz, nelems);
    }
    else if (isaStruct(t)) {
        ret = DtoGEPi(e->ptr, 0, 1);
        ret = DtoLoad(ret);

        size_t elembsz = getABITypeSize(ret->getType()->getContainedType(0));
        llvm::ConstantInt* elemsz = llvm::ConstantInt::get(DtoSize_t(), elembsz, false);

        LLValue* len = DtoGEPi(e->ptr, 0, 0);
        len = DtoLoad(len);
        sz = llvm::BinaryOperator::createMul(len,elemsz,"tmp",gIR->scopebb());
    }
    else {
        assert(0);
    }
    return ret;
}

void DtoArrayCopySlices(DSliceValue* dst, DSliceValue* src)
{
    Logger::println("ArrayCopySlices");

    LLValue *sz1,*sz2;
    LLValue* dstarr = get_slice_ptr(dst,sz1);
    LLValue* srcarr = get_slice_ptr(src,sz2);

    DtoMemCpy(dstarr, srcarr, sz1);
}

void DtoArrayCopyToSlice(DSliceValue* dst, DValue* src)
{
    Logger::println("ArrayCopyToSlice");

    LLValue* sz1;
    LLValue* dstarr = get_slice_ptr(dst,sz1);
    LLValue* srcarr = DtoArrayPtr(src);

    DtoMemCpy(dstarr, srcarr, sz1);
}

//////////////////////////////////////////////////////////////////////////////////////////
void DtoStaticArrayCopy(LLValue* dst, LLValue* src)
{
    Logger::println("StaticArrayCopy");

    size_t n = getABITypeSize(dst->getType()->getContainedType(0));
    DtoMemCpy(dst, src, DtoConstSize_t(n));
}

//////////////////////////////////////////////////////////////////////////////////////////
LLConstant* DtoConstSlice(LLConstant* dim, LLConstant* ptr)
{
    LLConstant* values[2] = { dim, ptr };
    return llvm::ConstantStruct::get(values, 2);
}

//////////////////////////////////////////////////////////////////////////////////////////
DSliceValue* DtoNewDynArray(Type* arrayType, DValue* dim, bool defaultInit)
{
    Logger::println("DtoNewDynArray : %s", arrayType->toChars());
    LOG_SCOPE;

    // typeinfo arg
    LLValue* arrayTypeInfo = DtoTypeInfoOf(arrayType);

    // dim arg
    assert(DtoType(dim->getType()) == DtoSize_t());
    LLValue* arrayLen = dim->getRVal();

    // get runtime function
    bool zeroInit = arrayType->toBasetype()->nextOf()->isZeroInit();
    LLFunction* fn = LLVM_D_GetRuntimeFunction(gIR->module, zeroInit ? "_d_newarrayT" : "_d_newarrayiT" );

    // call allocator
    LLValue* newptr = gIR->ir->CreateCall2(fn, arrayTypeInfo, arrayLen, ".gc_mem");

    // cast to wanted type
    const LLType* dstType = DtoType(arrayType)->getContainedType(1);
    if (newptr->getType() != dstType)
        newptr = DtoBitCast(newptr, dstType, ".gc_mem");

    Logger::cout() << "final ptr = " << *newptr << '\n';

#if 0
    if (defaultInit) {
        DValue* e = dty->defaultInit()->toElem(gIR);
        DtoArrayInit(newptr,dim,e->getRVal());
    }
#endif

    return new DSliceValue(arrayType, arrayLen, newptr);
}

//////////////////////////////////////////////////////////////////////////////////////////
DSliceValue* DtoNewMulDimDynArray(Type* arrayType, DValue** dims, size_t ndims, bool defaultInit)
{
    Logger::println("DtoNewMulDimDynArray : %s", arrayType->toChars());
    LOG_SCOPE;

    // typeinfo arg
    LLValue* arrayTypeInfo = DtoTypeInfoOf(arrayType);

    // get value type
    Type* vtype = arrayType->toBasetype();
    for (size_t i=0; i<ndims; ++i)
        vtype = vtype->nextOf();

    // get runtime function
    bool zeroInit = vtype->isZeroInit();
    LLFunction* fn = LLVM_D_GetRuntimeFunction(gIR->module, zeroInit ? "_d_newarraymT" : "_d_newarraymiT" );

    // build dims
    LLValue* dimsArg = new llvm::AllocaInst(DtoSize_t(), DtoConstUint(ndims), ".newdims", gIR->topallocapoint());
    LLValue* firstDim = NULL; 
    for (size_t i=0; i<ndims; ++i)
    {
        LLValue* dim = dims[i]->getRVal();
        if (!firstDim) firstDim = dim;
        DtoStore(dim, DtoGEPi1(dimsArg, i));
    }

    // call allocator
    LLValue* newptr = gIR->ir->CreateCall3(fn, arrayTypeInfo, DtoConstSize_t(ndims), dimsArg, ".gc_mem");

    // cast to wanted type
    const LLType* dstType = DtoType(arrayType)->getContainedType(1);
    if (newptr->getType() != dstType)
        newptr = DtoBitCast(newptr, dstType, ".gc_mem");

    Logger::cout() << "final ptr = " << *newptr << '\n';

#if 0
    if (defaultInit) {
        DValue* e = dty->defaultInit()->toElem(gIR);
        DtoArrayInit(newptr,dim,e->getRVal());
    }
#endif

    assert(firstDim);
    return new DSliceValue(arrayType, firstDim, newptr);
}

//////////////////////////////////////////////////////////////////////////////////////////
DSliceValue* DtoResizeDynArray(Type* arrayType, DValue* array, DValue* newdim)
{
    Logger::println("DtoResizeDynArray : %s", arrayType->toChars());
    LOG_SCOPE;

    assert(array);
    assert(newdim);
    assert(arrayType);
    assert(arrayType->toBasetype()->ty == Tarray);

    // decide on what runtime function to call based on whether the type is zero initialized
    bool zeroInit = arrayType->toBasetype()->next->isZeroInit();

    // call runtime
    LLFunction* fn = LLVM_D_GetRuntimeFunction(gIR->module, zeroInit ? "_d_arraysetlengthT" : "_d_arraysetlengthiT" );

    LLSmallVector<LLValue*,4> args;
    args.push_back(DtoTypeInfoOf(arrayType));
    args.push_back(newdim->getRVal());
    args.push_back(DtoArrayLen(array));

    LLValue* arrPtr = DtoArrayPtr(array);
    Logger::cout() << "arrPtr = " << *arrPtr << '\n';
    args.push_back(DtoBitCast(arrPtr, fn->getFunctionType()->getParamType(3), "tmp"));

    LLValue* newptr = gIR->ir->CreateCall(fn, args.begin(), args.end(), ".gc_mem");
    if (newptr->getType() != arrPtr->getType())
        newptr = DtoBitCast(newptr, arrPtr->getType(), ".gc_mem");

    return new DSliceValue(arrayType, newdim->getRVal(), newptr);
}

//////////////////////////////////////////////////////////////////////////////////////////
DSliceValue* DtoCatAssignElement(DValue* array, Expression* exp)
{
    Logger::println("DtoCatAssignElement");
    LOG_SCOPE;

    assert(array);

    LLValue* idx = DtoArrayLen(array);
    LLValue* one = DtoConstSize_t(1);
    LLValue* len = gIR->ir->CreateAdd(idx,one,"tmp");

    DValue* newdim = new DImValue(Type::tsize_t, len);
    DSliceValue* slice = DtoResizeDynArray(array->getType(), array, newdim);

    LLValue* ptr = slice->ptr;
    ptr = llvm::GetElementPtrInst::Create(ptr, idx, "tmp", gIR->scopebb());

    DValue* dptr = new DVarValue(exp->type, ptr, true);

    gIR->exps.push_back(IRExp(0,exp,dptr));
    DValue* e = exp->toElem(gIR);
    gIR->exps.pop_back();

    if (!e->inPlace())
        DtoAssign(dptr, e);

    return slice;
}

//////////////////////////////////////////////////////////////////////////////////////////
DSliceValue* DtoCatAssignArray(DValue* arr, Expression* exp)
{
    Logger::println("DtoCatAssignArray");
    LOG_SCOPE;

    DValue* e = exp->toElem(gIR);

    llvm::Value *len1, *len2, *src1, *src2, *res;

    len1 = DtoArrayLen(arr);
    len2 = DtoArrayLen(e);
    res = gIR->ir->CreateAdd(len1,len2,"tmp");

    DValue* newdim = new DImValue(Type::tsize_t, res);
    DSliceValue* slice = DtoResizeDynArray(arr->getType(), arr, newdim);

    src1 = slice->ptr;
    src2 = DtoArrayPtr(e);

    // advance ptr
    src1 = gIR->ir->CreateGEP(src1,len1,"tmp");

    // memcpy
    LLValue* elemSize = DtoConstSize_t(getABITypeSize(src2->getType()->getContainedType(0)));
    LLValue* bytelen = gIR->ir->CreateMul(len2, elemSize, "tmp");
    DtoMemCpy(src1,src2,bytelen);

    return slice;
}

//////////////////////////////////////////////////////////////////////////////////////////
DSliceValue* DtoCatArrays(Type* type, Expression* exp1, Expression* exp2)
{
    Logger::println("DtoCatArrays");
    LOG_SCOPE;

    Type* t1 = DtoDType(exp1->type);
    Type* t2 = DtoDType(exp2->type);

    assert(t1->ty == Tarray);
    assert(t1->ty == t2->ty);

    DValue* e1 = exp1->toElem(gIR);
    DValue* e2 = exp2->toElem(gIR);

    llvm::Value *len1, *len2, *src1, *src2, *res;

    len1 = DtoArrayLen(e1);
    len2 = DtoArrayLen(e2);
    res = gIR->ir->CreateAdd(len1,len2,"tmp");

    DValue* lenval = new DImValue(Type::tsize_t, res);
    DSliceValue* slice = DtoNewDynArray(type, lenval, false);
    LLValue* mem = slice->ptr;

    src1 = DtoArrayPtr(e1);
    src2 = DtoArrayPtr(e2);

    // first memcpy
    LLValue* elemSize = DtoConstSize_t(getABITypeSize(src1->getType()->getContainedType(0)));
    LLValue* bytelen = gIR->ir->CreateMul(len1, elemSize, "tmp");
    DtoMemCpy(mem,src1,bytelen);

    // second memcpy
    mem = gIR->ir->CreateGEP(mem,len1,"tmp");
    bytelen = gIR->ir->CreateMul(len2, elemSize, "tmp");
    DtoMemCpy(mem,src2,bytelen);

    return slice;
}

//////////////////////////////////////////////////////////////////////////////////////////
DSliceValue* DtoCatArrayElement(Type* type, Expression* exp1, Expression* exp2)
{
    Logger::println("DtoCatArrayElement");
    LOG_SCOPE;

    Type* t1 = DtoDType(exp1->type);
    Type* t2 = DtoDType(exp2->type);

    DValue* e1 = exp1->toElem(gIR);
    DValue* e2 = exp2->toElem(gIR);

    llvm::Value *len1, *src1, *res;

    // handle prefix case, eg. int~int[]
    if (t2->next && t1 == DtoDType(t2->next))
    {
        len1 = DtoArrayLen(e2);
        res = gIR->ir->CreateAdd(len1,DtoConstSize_t(1),"tmp");

        DValue* lenval = new DImValue(Type::tsize_t, res);
        DSliceValue* slice = DtoNewDynArray(type, lenval, false);
        LLValue* mem = slice->ptr;

        DVarValue* memval = new DVarValue(e1->getType(), mem, true);
        DtoAssign(memval, e1);

        src1 = DtoArrayPtr(e2);

        mem = gIR->ir->CreateGEP(mem,DtoConstSize_t(1),"tmp");

        LLValue* elemSize = DtoConstSize_t(getABITypeSize(src1->getType()->getContainedType(0)));
        LLValue* bytelen = gIR->ir->CreateMul(len1, elemSize, "tmp");
        DtoMemCpy(mem,src1,bytelen);


        return slice;
    }
    // handle suffix case, eg. int[]~int
    else
    {
        len1 = DtoArrayLen(e1);
        res = gIR->ir->CreateAdd(len1,DtoConstSize_t(1),"tmp");

        DValue* lenval = new DImValue(Type::tsize_t, res);
        DSliceValue* slice = DtoNewDynArray(type, lenval, false);
        LLValue* mem = slice->ptr;

        src1 = DtoArrayPtr(e1);

        LLValue* elemSize = DtoConstSize_t(getABITypeSize(src1->getType()->getContainedType(0)));
        LLValue* bytelen = gIR->ir->CreateMul(len1, elemSize, "tmp");
        DtoMemCpy(mem,src1,bytelen);

        mem = gIR->ir->CreateGEP(mem,len1,"tmp");
        DVarValue* memval = new DVarValue(e2->getType(), mem, true);
        DtoAssign(memval, e2);

        return slice;
    }
}

//////////////////////////////////////////////////////////////////////////////////////////
// helper for eq and cmp
static LLValue* DtoArrayEqCmp_impl(const char* func, DValue* l, DValue* r, bool useti)
{
    Logger::println("comparing arrays");
    LLFunction* fn = LLVM_D_GetRuntimeFunction(gIR->module, func);
    assert(fn);

    LLValue* lmem;
    LLValue* rmem;

    // cast static arrays to dynamic ones, this turns them into DSliceValues
    Logger::println("casting to dynamic arrays");
    Type* l_ty = DtoDType(l->getType());
    Type* r_ty = DtoDType(r->getType());
    assert(l_ty->next == r_ty->next);
    if ((l_ty->ty == Tsarray) || (r_ty->ty == Tsarray)) {
        Type* a_ty = l_ty->next->arrayOf();
        if (l_ty->ty == Tsarray)
            l = DtoCastArray(l, a_ty);
        if (r_ty->ty == Tsarray)
            r = DtoCastArray(r, a_ty);
    }

    Logger::println("giving storage");

    // we need to give slices storage
    if (l->isSlice()) {
        lmem = new llvm::AllocaInst(DtoType(l->getType()), "tmpparam", gIR->topallocapoint());
        DtoSetArray(lmem, DtoArrayLen(l), DtoArrayPtr(l));
    }
    // also null
    else if (l->isNull())
    {
        lmem = new llvm::AllocaInst(DtoType(l->getType()), "tmpparam", gIR->topallocapoint());
        DtoSetArray(lmem, llvm::Constant::getNullValue(DtoSize_t()), llvm::Constant::getNullValue(DtoType(l->getType()->next->pointerTo())));
    }
    else
        lmem = l->getRVal();

    // and for the rvalue ...
    // we need to give slices storage
    if (r->isSlice()) {
        rmem = new llvm::AllocaInst(DtoType(r->getType()), "tmpparam", gIR->topallocapoint());
        DtoSetArray(rmem, DtoArrayLen(r), DtoArrayPtr(r));
    }
    // also null
    else if (r->isNull())
    {
        rmem = new llvm::AllocaInst(DtoType(r->getType()), "tmpparam", gIR->topallocapoint());
        DtoSetArray(rmem, llvm::Constant::getNullValue(DtoSize_t()), llvm::Constant::getNullValue(DtoType(r->getType()->next->pointerTo())));
    }
    else
        rmem = r->getRVal();

    const LLType* pt = fn->getFunctionType()->getParamType(0);

    LLSmallVector<LLValue*, 3> args;
    Logger::cout() << "bitcasting to " << *pt << '\n';
    Logger::cout() << *lmem << '\n';
    Logger::cout() << *rmem << '\n';
    args.push_back(DtoBitCast(lmem,pt));
    args.push_back(DtoBitCast(rmem,pt));

    // pass array typeinfo ?
    if (useti) {
        Type* t = l->getType();
        LLValue* tival = DtoTypeInfoOf(t);
        // DtoTypeInfoOf only does declare, not enough in this case :/
        DtoForceConstInitDsymbol(t->vtinfo);
        Logger::cout() << "typeinfo decl: " << *tival << '\n';

        pt = fn->getFunctionType()->getParamType(2);
        args.push_back(DtoBitCast(tival, pt));
    }

    llvm::CallInst* call = gIR->ir->CreateCall(fn, args.begin(), args.end(), "tmp");

    // set param attrs
    llvm::PAListPtr palist;
    palist = palist.addAttr(1, llvm::ParamAttr::ByVal);
    palist = palist.addAttr(2, llvm::ParamAttr::ByVal);
    call->setParamAttrs(palist);

    return call;
}

//////////////////////////////////////////////////////////////////////////////////////////
LLValue* DtoArrayEquals(TOK op, DValue* l, DValue* r)
{
    LLValue* res = DtoArrayEqCmp_impl("_adEq", l, r, true);
    if (op == TOKnotequal)
        res = gIR->ir->CreateNot(res, "tmp");

    return res;
}

//////////////////////////////////////////////////////////////////////////////////////////
LLValue* DtoArrayCompare(TOK op, DValue* l, DValue* r)
{
    LLValue* res = 0;

    llvm::ICmpInst::Predicate cmpop;
    bool skip = false;

    switch(op)
    {
    case TOKlt:
    case TOKul:
        cmpop = llvm::ICmpInst::ICMP_SLT;
        break;
    case TOKle:
    case TOKule:
        cmpop = llvm::ICmpInst::ICMP_SLE;
        break;
    case TOKgt:
    case TOKug:
        cmpop = llvm::ICmpInst::ICMP_SGT;
        break;
    case TOKge:
    case TOKuge:
        cmpop = llvm::ICmpInst::ICMP_SGE;
        break;
    case TOKue:
        cmpop = llvm::ICmpInst::ICMP_EQ;
        break;
    case TOKlg:
        cmpop = llvm::ICmpInst::ICMP_NE;
        break;
    case TOKleg:
        skip = true;
        res = llvm::ConstantInt::getTrue();
        break;
    case TOKunord:
        skip = true;
        res = llvm::ConstantInt::getFalse();
        break;

    default:
        assert(0);
    }

    if (!skip)
    {
        Type* t = DtoDType(DtoDType(l->getType())->next);
        if (t->ty == Tchar)
            res = DtoArrayEqCmp_impl("_adCmpChar", l, r, false);
        else
            res = DtoArrayEqCmp_impl("_adCmp", l, r, true);
        res = new llvm::ICmpInst(cmpop, res, DtoConstInt(0), "tmp", gIR->scopebb());
    }

    assert(res);
    return res;
}

//////////////////////////////////////////////////////////////////////////////////////////
LLValue* DtoArrayCastLength(LLValue* len, const LLType* elemty, const LLType* newelemty)
{
    Logger::println("DtoArrayCastLength");
    LOG_SCOPE;

    assert(len);
    assert(elemty);
    assert(newelemty);

    size_t esz = getABITypeSize(elemty);
    size_t nsz = getABITypeSize(newelemty);
    if (esz == nsz)
        return len;

    LLSmallVector<LLValue*, 3> args;
    args.push_back(len);
    args.push_back(llvm::ConstantInt::get(DtoSize_t(), esz, false));
    args.push_back(llvm::ConstantInt::get(DtoSize_t(), nsz, false));

    LLFunction* fn = LLVM_D_GetRuntimeFunction(gIR->module, "_d_array_cast_len");
    return llvm::CallInst::Create(fn, args.begin(), args.end(), "tmp", gIR->scopebb());
}

//////////////////////////////////////////////////////////////////////////////////////////
LLValue* DtoDynArrayIs(TOK op, LLValue* l, LLValue* r)
{
    llvm::ICmpInst::Predicate pred = (op == TOKidentity) ? llvm::ICmpInst::ICMP_EQ : llvm::ICmpInst::ICMP_NE;

    if (r == NULL) {
        LLValue* ll = gIR->ir->CreateLoad(DtoGEPi(l, 0,0),"tmp");
        LLValue* rl = llvm::Constant::getNullValue(ll->getType());//DtoConstSize_t(0);
        LLValue* b1 = gIR->ir->CreateICmp(pred,ll,rl,"tmp");

        LLValue* lp = gIR->ir->CreateLoad(DtoGEPi(l, 0,1),"tmp");
        const LLPointerType* pty = isaPointer(lp->getType());
        LLValue* rp = llvm::ConstantPointerNull::get(pty);
        LLValue* b2 = gIR->ir->CreateICmp(pred,lp,rp,"tmp");

        LLValue* b = gIR->ir->CreateAnd(b1,b2,"tmp");
        return b;
    }
    else {
        assert(l->getType() == r->getType());

        LLValue* ll = gIR->ir->CreateLoad(DtoGEPi(l, 0,0),"tmp");
        LLValue* rl = gIR->ir->CreateLoad(DtoGEPi(r, 0,0),"tmp");
        LLValue* b1 = gIR->ir->CreateICmp(pred,ll,rl,"tmp");

        LLValue* lp = gIR->ir->CreateLoad(DtoGEPi(l, 0,1),"tmp");
        LLValue* rp = gIR->ir->CreateLoad(DtoGEPi(r, 0,1),"tmp");
        LLValue* b2 = gIR->ir->CreateICmp(pred,lp,rp,"tmp");

        LLValue* b = gIR->ir->CreateAnd(b1,b2,"tmp");
        return b;
    }
}

//////////////////////////////////////////////////////////////////////////////////////////
LLConstant* DtoConstStaticArray(const LLType* t, LLConstant* c)
{
    const LLArrayType* at = isaArray(t);
    assert(at);

    if (isaArray(at->getElementType()))
    {
        c = DtoConstStaticArray(at->getElementType(), c);
    }
    else {
        assert(at->getElementType() == c->getType());
    }
    std::vector<LLConstant*> initvals;
    initvals.resize(at->getNumElements(), c);
    return llvm::ConstantArray::get(at, initvals);
}

//////////////////////////////////////////////////////////////////////////////////////////
LLValue* DtoArrayLen(DValue* v)
{
    Logger::println("DtoArrayLen");
    LOG_SCOPE;

    Type* t = DtoDType(v->getType());
    if (t->ty == Tarray) {
        if (DSliceValue* s = v->isSlice()) {
            if (s->len) {
                return s->len;
            }
            const LLArrayType* arrTy = isaArray(s->ptr->getType()->getContainedType(0));
            if (arrTy)
                return DtoConstSize_t(arrTy->getNumElements());
            else
                return DtoLoad(DtoGEPi(s->ptr, 0,0));
        }
        return DtoLoad(DtoGEPi(v->getRVal(), 0,0));
    }
    else if (t->ty == Tsarray) {
        assert(!v->isSlice());
        LLValue* rv = v->getRVal();
        Logger::cout() << "casting: " << *rv << '\n';
        const LLArrayType* t = isaArray(rv->getType()->getContainedType(0));
        return DtoConstSize_t(t->getNumElements());
    }
    assert(0);
    return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////
LLValue* DtoArrayPtr(DValue* v)
{
    Logger::println("DtoArrayPtr");
    LOG_SCOPE;

    Type* t = DtoDType(v->getType());
    if (t->ty == Tarray) {
        if (DSliceValue* s = v->isSlice()) {
            if (s->len) return s->ptr;
            const LLType* t = s->ptr->getType()->getContainedType(0);
            Logger::cout() << "ptr of full slice: " << *s->ptr << '\n';
            const LLArrayType* arrTy = isaArray(s->ptr->getType()->getContainedType(0));
            if (arrTy)
                return DtoGEPi(s->ptr, 0,0);
            else
                return DtoLoad(DtoGEPi(s->ptr, 0,1));
        }
        return DtoLoad(DtoGEPi(v->getRVal(), 0,1));
    }
    else if (t->ty == Tsarray) {
        return DtoGEPi(v->getRVal(), 0,0);
    }
    assert(0);
    return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////
DValue* DtoCastArray(DValue* u, Type* to)
{
    Logger::println("DtoCastArray");
    LOG_SCOPE;

    const LLType* tolltype = DtoType(to);

    Type* totype = DtoDType(to);
    Type* fromtype = DtoDType(u->getType());
    assert(fromtype->ty == Tarray || fromtype->ty == Tsarray);

    LLValue* rval;
    LLValue* rval2;
    bool isslice = false;

    Logger::cout() << "from array or sarray" << '\n';
    if (totype->ty == Tpointer) {
        Logger::cout() << "to pointer" << '\n';
        rval = DtoArrayPtr(u);
        if (rval->getType() != tolltype)
            rval = gIR->ir->CreateBitCast(rval, tolltype, "tmp");
    }
    else if (totype->ty == Tarray) {
        Logger::cout() << "to array" << '\n';
        const LLType* ptrty = DtoType(totype->next);
        if (ptrty == LLType::VoidTy)
            ptrty = LLType::Int8Ty;
        ptrty = getPtrToType(ptrty);

        const LLType* ety = DtoType(fromtype->next);
        if (ety == LLType::VoidTy)
            ety = LLType::Int8Ty;

        if (DSliceValue* usl = u->isSlice()) {
            Logger::println("from slice");
            Logger::cout() << "from: " << *usl->ptr << " to: " << *ptrty << '\n';
            rval = DtoBitCast(usl->ptr, ptrty);
            if (fromtype->next->size() == totype->next->size())
                rval2 = DtoArrayLen(usl);
            else
                rval2 = DtoArrayCastLength(DtoArrayLen(usl), ety, ptrty->getContainedType(0));
        }
        else {
            LLValue* uval = u->getRVal();
            if (fromtype->ty == Tsarray) {
                Logger::cout() << "uvalTy = " << *uval->getType() << '\n';
                assert(isaPointer(uval->getType()));
                const LLArrayType* arrty = isaArray(uval->getType()->getContainedType(0));
                rval2 = llvm::ConstantInt::get(DtoSize_t(), arrty->getNumElements(), false);
                rval2 = DtoArrayCastLength(rval2, ety, ptrty->getContainedType(0));
                rval = DtoBitCast(uval, ptrty);
            }
            else {
                LLValue* zero = llvm::ConstantInt::get(LLType::Int32Ty, 0, false);
                LLValue* one = llvm::ConstantInt::get(LLType::Int32Ty, 1, false);
                rval2 = DtoGEP(uval,zero,zero);
                rval2 = DtoLoad(rval2);
                rval2 = DtoArrayCastLength(rval2, ety, ptrty->getContainedType(0));

                rval = DtoGEP(uval,zero,one);
                rval = DtoLoad(rval);
                //Logger::cout() << *e->mem->getType() << '|' << *ptrty << '\n';
                rval = DtoBitCast(rval, ptrty);
            }
        }
        isslice = true;
    }
    else if (totype->ty == Tsarray) {
        Logger::cout() << "to sarray" << '\n';
        assert(0);
    }
    else {
        assert(0);
    }

    if (isslice) {
        Logger::println("isslice");
        return new DSliceValue(to, rval2, rval);
    }

    return new DImValue(to, rval);
}
