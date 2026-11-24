/*
 * Copyright (c) 2026 The kido authors
 * SPDX-License-Identifier: Apache-2.0
 */

#import <objc/NSObject.h>
#import <objc/message.h>
#import <objc/runtime.h>
#include <Block.h>
#include <mach-o/dyld.h>
#include <algorithm>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <typeinfo>
#include <vector>

#include "../libsystem/check.h"

@interface Counter : NSObject {
@public
    int total;
}
- (int)add:(int)n;
@end

@implementation Counter
- (int)add:(int)n
{
    total += n;
    return total;
}
@end

static int
twice(id self, SEL cmd, int n)
{
    (void)self;
    (void)cmd;
    return 2 * n;
}

struct Shape {
    virtual ~Shape() = default;
    virtual int area() const = 0;
};

struct Square : Shape {
    int side;
    explicit Square(int s) : side(s) {}
    int area() const override { return side * side; }
};

struct Rect : Shape {
    int w, h;
    Rect(int w_, int h_) : w(w_), h(h_) {}
    int area() const override { return w * h; }
};

static int copies, destroys;

struct Tracked {
    int value = 5;
    Tracked() = default;
    Tracked(const Tracked &other) : value(other.value) { copies++; }
    ~Tracked() { destroys++; }
};

static int
byref_object(void)
{
    int seen;
    {
        __block Tracked t;
        int (^b)(void) = ^{ return t.value; };
        int (^h)(void) = Block_copy(b);
        seen = h();
        Block_release(h);
    }
    return seen;
}

static bool
loaded(const char *path)
{
    for (uint32_t i = 0; i < _dyld_image_count(); i++) {
        if (strcmp(_dyld_get_image_name(i), path) == 0) {
            return true;
        }
    }
    return false;
}

int
main(void)
{
    CHECK(loaded("/usr/lib/libobjc.A.dylib"));
    CHECK(loaded("/usr/lib/libc++.1.dylib"));
    CHECK(loaded("/usr/lib/libc++abi.dylib"));

    Counter *c = [[Counter alloc] init];
    int last = 0;
    for (int i = 0; i < 1000; i++) {
        last = [c add:1];
    }
    CHECK(last == 1000);
    CHECK([c isKindOfClass:[NSObject class]] && [c respondsToSelector:@selector(add:)]);
    CHECK(class_addMethod([Counter class], sel_registerName("twice:"), (IMP)twice, "i@:i"));
    CHECK(((int (*)(id, SEL, int))objc_msgSend)(c, sel_registerName("twice:"), 21) == 42);
    Class sub = objc_allocateClassPair([Counter class], "KidoCounterSub", 0);
    CHECK(sub != Nil);
    if (sub != Nil) {
        objc_registerClassPair(sub);
        id s = [[sub alloc] init];
        CHECK([s add:7] == 7 && [s isKindOfClass:[Counter class]] && class_getSuperclass(sub) == [Counter class]);
        [s release];
    }
    [c release];

    std::vector<std::unique_ptr<Shape>> shapes;
    shapes.push_back(std::make_unique<Square>(3));
    shapes.push_back(std::make_unique<Rect>(2, 5));
    int total = 0;
    for (const auto &s : shapes) {
        total += s->area();
    }
    CHECK(total == 19);
    CHECK(dynamic_cast<Square *>(shapes[0].get()) != nullptr && dynamic_cast<Square *>(shapes[1].get()) == nullptr);
    const Shape &second = *shapes[1];
    CHECK(typeid(second) == typeid(Rect));
    std::map<std::string, int> m{{"b", 2}, {"a", 1}};
    CHECK(m.begin()->first == "a" && m["b"] == 2);
    std::function<int(int)> f = [k = 3](int x) { return x * k; };
    CHECK(f(4) == 12);
    std::vector<int> v{5, 1, 4};
    std::sort(v.begin(), v.end(), [](int a, int b) { return a > b; });
    CHECK(v[0] == 5 && v[2] == 1);
    std::string str = "pointer";
    str += " authentication";
    CHECK(str.size() == 22 && str.find("auth") == 8);
    std::shared_ptr<int> shared(new int(7), [](int *p) { delete p; });
    CHECK(*shared == 7 && shared.use_count() == 1);

    CHECK(byref_object() == 5 && copies >= 1 && destroys == copies + 1);
#if __arm64e__
    return checks_done("objc and c++ arm64e");
#else
    return checks_done("objc and c++ arm64");
#endif
}
