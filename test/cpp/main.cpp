#include <iostream>
#include <unistd.h>
#include <chrono>

#include "RingBufWrapper.hpp"

class A {
public:
    int i = 1;
    char buf[1024];
    A(int _i) : i(_i) {}
    A() : i(-1) {}
    A &operator++()
    { 
        ++i;
        return *this;
    }
};

std::ostream &operator<<(std::ostream &cout, const A &tmp)
{
    std::cout << tmp.i;
    return cout;
}

namespace RW = RingBufWrapper;
using Obj = A;
constexpr auto ObjNum = 8;
constexpr auto SHM = "/dev/shm/test";
constexpr auto Loop = 100000;


int cb1(void *p, void *args)
{
    // std::cout << *reinterpret_cast<Obj *>(p) << std::endl;
    auto i = rand();
    return i;
}

template<typename T>
void func(Obj &o, int n)
{
    auto cb = [&](void *p) noexcept -> int {
        // std::cout << *reinterpret_cast<Obj *>(p) << std::endl;
        auto i = rand();
        return i;
    };
    int x = 0;
    T r;
    int rc = r.Init(SHM, MAP_SHM | MAP_NEW, 0);
    if (rc < 0) {
        printf("%s\n", r.Get_RingBuf_strerror(rc));
        return;
    }
    decltype(r) s = r;
    Obj j;
    for (int i = 0; i < n; ++i) {
        r.Push(o);

        r.Pop(j); 
        // x = cb(nullptr);
        // x = s.Pop_w_cb(cb1, nullptr);
        // x = s.Pop_w_cb(cb);
        // x = s.Pop_MpmcMpscRingBuf(j);

        // int y = x + rand();
        // std::cout << y << std::endl;
    }
    unlink(SHM);
}

template<typename T>
void check_rc(T &r, int rc)
{
    if (rc < 0) {
        printf("%s\n", r.Get_RingBuf_strerror(rc));
    }
}

int main()
{
    Obj o = 9;
    // auto start = std::chrono::steady_clock::now();
    func<RW::RingBuf<RW::RingBufType::Spsc, Obj, ObjNum>>(o, Loop);
    func<RW::RingBuf<RW::RingBufType::Mpsc, Obj, ObjNum>>(o, Loop);
    func<RW::RingBuf<RW::RingBufType::Mpmc, Obj, ObjNum>>(o, Loop);
    func<RW::RingBuf<RW::RingBufType::Block, Obj, ObjNum>>(o, Loop);
    // auto end = std::chrono::steady_clock::now();
    // std::cout << std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() << std::endl;

    int rc = 0;
    auto p = &o;
    // RW::RingBuf<RW::RingBufType::Block, Obj, ObjNum> r;
    // check_rc(r, r.Init(SHM, MAP_SHM | MAP_NEW, 0));
    // r.Pop(*p);
    // r.~RingBuf();
    RW::RingBuf<RW::RingBufType::Mpmc, Obj, ObjNum> r1;
    check_rc(r1, r1.Init(SHM, MAP_SHM | MAP_NEW, 0));
    r1.Pop(*p);
    r1.~RingBuf();
    RW::RingBuf<RW::RingBufType::Mpsc, Obj, ObjNum> r2;
    check_rc(r2, r2.Init(SHM, MAP_SHM | MAP_NEW, 0));
    r2.Pop(*p);
    r2.~RingBuf();
    RW::RingBuf<RW::RingBufType::Spsc, Obj, ObjNum> r3;
    check_rc(r3, r3.Init(SHM, MAP_SHM | MAP_NEW, 0));
    r3.Pop(*p);
    r3.~RingBuf();
    
    // int rc = r.Init(SHM, MAP_NEW | MAP_SHM, 0);
    // if (rc < 0) {
    //     std::cout << r.Get_RingBuf_strerror(errno) << std::endl;
    //     return rc;
    // }

    // for (uint8_t i = 0; i < 3; ++i) {
    //     if (r.Push(o) < 0) {
    //         std::cout << r.Get_RingBuf_strerror(errno) << std::endl;
    //         return rc;
    //     }
    // }

    // int x = 10;
    // double y = 10;
    // char z = 'Z';
    // struct ARGS {
    //     int &x;
    //     double &y;
    //     char &c;
    // } arg{x, y, z};

    // Obj p;
    // auto bb = [](void *obj, void *args) {
    // // auto bb = [](void *obj, ARGS *args) {
    //     puts("===");
    //     std::cout << "call bb" << std::endl;
    //     std::cout << ((Obj*)obj)->i << std::endl;
    //     auto arg = reinterpret_cast<ARGS*>(args);
    //     printf("%p, %p, %p\n", &arg->x, &arg->y, &arg->c);
    //     return 0;
    // };

    // auto cc = [&](void *obj, auto &&i, auto &&d, auto &&c) {
    //     puts("===");
    //     std::cout << "call cc" << std::endl;
    //     std::cout << ((Obj*)obj)->i << std::endl;
    //     printf("%p, %p, %p\n", &i, &d, &c);
    //     return 0;
    // };
    // printf("%p, %p, %p\n", &x, &y, &z);
    // r.Pop_w_cb(bb, &arg);
    // r.Pop_w_cb(cc, x, y, z);
    // r.Pop(p);

    unlink(SHM);
    return 0;
}