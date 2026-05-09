#ifndef MEMORY_MANAGER_H
#define MEMORY_MANAGER_H

#include <iostream>

#include "LinearPoolAllocator.h"
#include "StackPoolAllocator.h"

#include <vulkan/vulkan.h>
#include <vma/vk_mem_alloc.h>


namespace Loops::Memory
{
    class MemoryManager
    {
    private:

        static VmaAllocator sm_vma;
    public:

        static void InitVMA(const VkPhysicalDevice& physicalDevice, const VkDevice& logicalDevice, const VkInstance& instance);
        static void DeInitVMA();
        static VmaAllocator& GetVmaAllocator();

        MemoryManager()
        {
            /*
            class Test
            {
            private:
                int age;
                std::string name;
            public:
                Test(int b , std::string c):age(b), name(c)
                {
                    std::cout << "constructed "<< name <<std::endl<<std::flush;
                }

                ~Test()
                {
                    std::cout << "Destructed " << name << std::endl << std::flush;
                }
            };
            
            {
                LinearPoolAllocator linearPool(sizeof(Test) * 3);
                //Test* t1 = reinterpret_cast<Test*>(linearPool.Allocate(sizeof(Test), alignof(Test)));
                {
                    Test* t1 = linearPool.Allocate<Test>(8, "SUZZANE");
                    linearPool.DeAllocate<Test>(t1);
                }

                {
                    Test* t1 = linearPool.Allocate<Test>(8, "SUZZANE");
                    linearPool.DeAllocate<Test>(t1);
                }

                {
                    Test* t1 = linearPool.Allocate<Test>(8, "SUZZANE");
                    linearPool.DeAllocate<Test>(t1);
                }
            }

            {
                StackPoolAllocator stackPool(sizeof(Test) * 3);
                {
                    Test* t1 = stackPool.Allocate<Test>(8, "SUZZANE");
                    Test* t2 = stackPool.Allocate<Test>(1, "CHECK");
                    stackPool.DeAllocate<Test>(t2);
                    stackPool.DeAllocate<Test>(t1);
                    Test* t3 = stackPool.Allocate<Test>(5, "TEST");
                    stackPool.DeAllocate<Test>(t3);
                }
            }
            */
        }
    };
}
#endif // !MEMORY_MANAGER_H

/*

============================== Simple allocator example with vector

#include <memory>
#include <iostream>
#include <vector>
#include <string>

template<typename T>
class SimpleAllocator {
public:
    using value_type = T;

    SimpleAllocator() = default;

    template<typename U>
    constexpr SimpleAllocator(const SimpleAllocator<U>&) noexcept {}

    T* allocate(std::size_t n) {
        std::cout << "Allocating " << n * sizeof(T) << " bytes" << std::endl;
        if (n > std::allocator_traits<SimpleAllocator>::max_size(*this)) {
            throw std::bad_alloc();
        }
        return static_cast<T*>(::operator new(n * sizeof(T)));
    }

    void deallocate(T* p, std::size_t) noexcept {
        std::cout << "Deallocating memory : " << p << std::endl;
        ::operator delete(p);
    }

    template<typename U, typename... Args>
    void construct(U* p, Args&&... args) {
        std::cout << "Constructing element at " <<p << std::endl;
        new(p) U(std::forward<Args>(args)...);
    }

    template<typename U>
    void destroy(U* p) noexcept {
        std::cout << "Destroying element at " <<p << std::endl;
        p->~U();
    }

    friend bool operator==(const SimpleAllocator&, const SimpleAllocator&) { return true; }
    friend bool operator!=(const SimpleAllocator&, const SimpleAllocator&) { return false; }
};

class Test
{
 private:
    int m_value = 0;
    std::string m_name;
 public:
    Test(int val, std::string name): m_value(val), m_name(name)
    {

    }
};

int main() {

    std::cout << "============" << std::endl;
    {
        SimpleAllocator<Test> testAlloc;

        std::vector<Test, SimpleAllocator<Test>> vec2(testAlloc);

        std::cout << "push one ============" << std::endl;
        vec2.push_back({1, "one"});
        std::cout << "push two ============" << std::endl;
        vec2.push_back({2, "two"});
        std::cout << "push three ============" << std::endl;
        vec2.push_back({3, "three"});

        std::cout << "Clearing vector:" << std::endl;
        vec2.clear();
        std::cout << "Exiting:" << std::endl;
    }

    return 0;
}

// ======================================== OUTPUT

Adding elements to vector:
push one ============
Allocating 40 bytes
Constructing element at 0x39f2e6c0
push two ============
Allocating 80 bytes
Constructing element at 0x39f2e718
Constructing element at 0x39f2e6f0
Destroying element at 0x39f2e6c0
Deallocating memory : 0x39f2e6c0
push three ============
Allocating 160 bytes
Constructing element at 0x39f2e7a0
Constructing element at 0x39f2e750
Constructing element at 0x39f2e778
Destroying element at 0x39f2e6f0
Destroying element at 0x39f2e718
Deallocating memory : 0x39f2e6f0
Clearing vector:
Destroying element at 0x39f2e750
Destroying element at 0x39f2e778
Destroying element at 0x39f2e7a0
Exiting:
Deallocating memory : 0x39f2e750

*/
