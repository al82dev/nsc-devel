#pragma once
extern int destroyed;
struct Child {
    int value;
    Child() : value(17) {}
    ~Child() { ++destroyed; }
};
struct Parent { Child child; };
