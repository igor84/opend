// REQUIRED_ARGS:
// PERMUTE_ARGS: -mcpu=native -inline -O

version (D_SIMD)
{

import core.simd;
import core.stdc.string;

alias TypeTuple(T...) = T;

/*****************************************/
// https://issues.dlang.org/show_bug.cgi?id=21474

struct along
{
    long[1] arr;
}

int4 to_m128i(along a)
{
    long2 r;
    r[0] = a.arr[0];
    return cast(int4)r;
}

void test21474()
{
    along a;
    a.arr[0] = 0x1234_5678_9ABC_DEF0;
    int4 i4 = to_m128i(a);
    assert(i4[0] == 0x9ABC_DEF0);
    assert(i4[1] == 0x1234_5678);
    assert(i4[2] == 0);
    assert(i4[3] == 0);
}

int4 cmpss_repro(float4 a)
{
    int4 result;
    result.ptr[0] = (1 > a.array[0]) ? -1 : 0;
    return result;
}

/*****************************************/
// https://issues.dlang.org/show_bug.cgi?id=23461

int4 and(int a) { return a && true; }
int4 or(int a) { return a || false; }

void test23461()
{
    int4 x = and(3);
    assert(x.array == [1,1,1,1]);
    x = and(0);
    assert(x.array == [0,0,0,0]);

    x = or(3);
    assert(x.array == [1,1,1,1]);
    x = or(0);
    assert(x.array == [0,0,0,0]);
}

/*****************************************/

int main()
{
    test21474();
    test23461();

    return 0;
}

}
else
{

int main() { return 0; }

}
