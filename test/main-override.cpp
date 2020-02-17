#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdint.h>
#include <new>
#include <vector>
#include <thread>
#include <mimalloc.h>
// #include <mimalloc-new-delete.h>
#include <mimalloc-override.h>

#ifdef _WIN32
#include <windows.h>
static void msleep(unsigned long msecs) { Sleep(msecs); }
#else
#include <unistd.h>
static void msleep(unsigned long msecs) { usleep(msecs * 1000UL); }
#endif

static void heap_no_delete();
static void heap_late_free();
static void various_tests();
static void dangling_ptr_write();

int main() {
  mi_stats_reset();  // ignore earlier allocations
  // heap_no_delete();  // issue #202
  // heap_late_free();  // issue #204
  // dangling_ptr_write();
  various_tests();
  mi_stats_print(NULL);
  return 0;
}

static void* p = malloc(8);

void free_p() {
  free(p);
  return;
}

class Test {
private:
  int i;
public:
  Test(int x) { i = x; }
  ~Test() { }
};


static void various_tests() {
  atexit(free_p);
  void* p1 = malloc(78);
  void* p2 = mi_malloc_aligned(16,24);
  free(p1);
  p1 = malloc(8);
  char* s = _strdup("hello\n");

  //char* s = _strdup("hello\n");
  //char* buf = NULL;
  //size_t len;
  //_dupenv_s(&buf,&len,"MIMALLOC_VERBOSE");
  //mi_free(buf);

  mi_free(p2);
  p2 = malloc(16);
  p1 = realloc(p1, 32);
  free(p1);
  free(p2);
  mi_free(s);
  s[0] = 0;
  Test* t = new Test(42);
  delete t;
  // t = new(std::nothrow) Test(42);   // does not work with overriding :-(
  t = new Test(42);
  delete t;
}

static void dangling_ptr_write() {
  for (int i = 0; i < 1000; i++) {
    uint8_t* p;
    if ((i & 1) == 0) {   // do ==0 or ==1 to get either malloc or new allocation
      p = (uint8_t*)malloc(16);
      free(p);
    }
    else {
      p = new uint8_t[16];
      // delete p;   // delete sets the pointer to an invalid value generally
      free(p);
    }
    p[0] = 0;
  }
}

class Static {
private:
  void* p;
public:
  Static() {
    p = malloc(64);
    return;
  }
  ~Static() {
    free(p);
    return;
  }
};

static Static s = Static();


static bool test_stl_allocator1() {
  std::vector<int, mi_stl_allocator<int> > vec;
  vec.push_back(1);
  vec.pop_back();
  return vec.size() == 0;
}

struct some_struct { int i; int j; double z; };

bool test_stl_allocator2() {
  std::vector<some_struct, mi_stl_allocator<some_struct> > vec;
  vec.push_back(some_struct());
  vec.pop_back();
  return vec.size() == 0;
}



// Issue #202
static void heap_no_delete_worker() {
  mi_heap_t* heap = mi_heap_new();
  void* q = mi_heap_malloc(heap,1024);
  // mi_heap_delete(heap); // uncomment to prevent assertion
}

static void heap_no_delete() {
  auto t1 = std::thread(heap_no_delete_worker);
  t1.join();
}


// Issue #204
volatile void* global_p;

static void t1main() {
  mi_heap_t* heap = mi_heap_new();
  global_p = mi_heap_malloc(heap, 1024);
  mi_heap_delete(heap);
}

static void heap_late_free() {
  auto t1 = std::thread(t1main);

  msleep(2000);
  assert(global_p);
  mi_free((void*)global_p);

  t1.join();
}
