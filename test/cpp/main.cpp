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
constexpr auto ObjNum = 1024;
constexpr auto SHM = "/dev/shm/test";
constexpr auto Loop = 100000;

int cb(const void *p, void *args)
{
    // std::cout << *reinterpret_cast<Obj *>(p) << std::endl;
    auto i = rand();
    return i;
}

int cbb(const void *p)
{
    // std::cout << *reinterpret_cast<Obj *>(p) << std::endl;
    auto i = rand();
    return i;
}

int cbbb(void *p, void *args)
{
    // std::cout << *reinterpret_cast<Obj *>(p) << std::endl;
    auto i = rand();
    return i;
}

int cbbbb(const void *p,  int *a, int *b)
{
    // std::cout << *reinterpret_cast<Obj *>(p) << std::endl;
    auto i = rand();
    return i;
}

int cbbbbb(int *p, int *a, int *b)
{
    // std::cout << *reinterpret_cast<Obj *>(p) << std::endl;
    auto i = rand();
    return i;
}

template<typename T>
void func(Obj &o, int n)
{
    auto cb1 = [&](const void *p, void *args) noexcept -> int {
        // std::cout << *reinterpret_cast<Obj *>(p) << std::endl;
        auto i = rand();
        return i;
    };

    auto cb2 = [](const void *p, void *args) noexcept -> int {
        // std::cout << *reinterpret_cast<Obj *>(p) << std::endl;
        auto i = rand();
        return i;
    };

    auto cb3 = [&](const void *p) noexcept -> int {
        // std::cout << *reinterpret_cast<Obj *>(p) << std::endl;
        auto i = rand();
        return i;
    };

    auto cb4 = [&](void *p) noexcept -> int {
        // std::cout << *reinterpret_cast<Obj *>(p) << std::endl;
        auto i = rand();
        return i;
    };

    auto cb5 = [](void *p) noexcept -> int {
        // std::cout << *reinterpret_cast<Obj *>(p) << std::endl;
        auto i = rand();
        return i;
    };

    auto cb6 = []() noexcept -> int {
        // std::cout << *reinterpret_cast<Obj *>(p) << std::endl;
        auto i = rand();
        return i;
    };

    auto cb7 = [](int *p, void *args) noexcept -> int {
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
    Obj j, k;
    char buf[2048] = {};
    for (int i = 0; i < n; ++i) {
        x = r.Push(o);
        x = r.Push(o);
        x = r.Push(o);
        x = r.Push(o);
        x = r.Push(o);
        x = r.Push(o);
        x = r.Push(o);
        x = r.Push(o);
        x = r.Push(o);
        x = r.Push(o);
        x = r.Push(o);

        x = r.Pop(j); 
        x = r.Pop(buf, sizeof(Obj));
        x = s.Pop_w_cb(cb, &j); // fast
        x = s.Pop_w_cb(cbb); // none fast
        x = s.Pop_w_cb(cbbb, &j); // none fast
        x = s.Pop_w_cb(cbbbb, &i, &i); // none fast
        // x = s.Pop_w_cb(cbbbbb, &i, &i); // 第一個參數不是 void* 的 callback 不支援

        x = s.Pop_w_cb(cb1, &j); // none fast
        x = s.Pop_w_cb(cb2, &j); // fast
        x = s.Pop_w_cb(cb3); // none fast
        x = s.Pop_w_cb(cb4); //none fast
        x = s.Pop_w_cb(cb5); // none fast
        // x = s.Pop_w_cb(cb6); // 沒有參數的 callback 不支援
        // x = s.Pop_w_cb(cb7, &j); // 第一個參數不是 void* 的 callback 不支援
    }
    unlink(SHM);
}

template <typename T>
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
    RW::RingBuf<RW::RingBufType::Block, Obj, ObjNum> r;
    check_rc(r, r.Init(SHM, MAP_SHM | MAP_NEW, 0));
    r.Push(&o, sizeof(o));
    r.Push(o);
    r.Pop(*p);
    r.~RingBuf();

    RW::RingBuf<RW::RingBufType::Mpmc, Obj, ObjNum> r1;
    check_rc(r1, r1.Init(SHM, MAP_SHM | MAP_NEW, 0));
    r1.Push(&o, sizeof(o));
    r1.Push(o);
    r1.Pop(*p);
    r1.~RingBuf();

    RW::RingBuf<RW::RingBufType::Mpsc, Obj, ObjNum> r2;
    check_rc(r2, r2.Init(SHM, MAP_SHM | MAP_NEW, 0));
    r2.Push(&o, sizeof(o));
    r2.Push(o);
    r2.Pop(*p);
    r2.~RingBuf();

    RW::RingBuf<RW::RingBufType::Spsc, Obj, ObjNum> r3;
    check_rc(r3, r3.Init(SHM, MAP_SHM | MAP_NEW, 0));
    r3.Push(&o, sizeof(o));
    r3.Push(o);
    auto x = r3.BeginPush();
    r3.EndPush();
    r3.BeginPop();
    r3.EndPop();
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
