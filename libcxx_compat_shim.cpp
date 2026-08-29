/* libjxl.a/libhwy.a (this filter's own bundled C++ static libraries) call
 * a handful of libc++ std::ranges::sort/partial_sort/heap-algorithm and
 * std::string internals that are header-only templates: they are not part
 * of the precompiled libc++.a shipped with emsdk, they only exist in the
 * link if some translation unit instantiates them exactly this way.
 * Whatever originally compiled libjxl.a/libhwy.a apparently did so in a way
 * that left these as separate (non-inlined) symbols, but plain -O2/-Oz
 * compilation of this filter's own dec_jxl.c/reframe_jxl.c (plain C, no
 * C++ at all) never triggers them, so wasm-ld leaves them as unresolved
 * "env" imports this filter's SIDE_MODULE link expects the main module to
 * provide - which is wrong, since this is purely an internal dependency of
 * this filter's own bundled libjxl/highway code, not something generic
 * that belongs in solver_minimal.
 *
 * This file forces the compiler to emit those exact instantiations so
 * wasm-ld resolves them locally, inside libjxl_1.wasm itself, instead of
 * importing them. The `volatile` guard is never true, so the actual
 * sort/string work never runs at load time - this exists purely to make
 * the compiler generate (and the linker keep) the symbols, not to do
 * anything at runtime.
 *
 * libc++'s internal sort/heap/string helpers are declared
 * _LIBCPP_HIDE_FROM_ABI, i.e. __attribute__((visibility("hidden"))) - a
 * hard "never exportable" marker wasm-ld honors even with an explicit
 * -Wl,--export= request. _LIBCPP_DISABLE_VISIBILITY_ANNOTATIONS is
 * libc++'s own supported escape hatch (see __config) that turns every
 * _LIBCPP_VISIBILITY(...) annotation into a no-op for this translation
 * unit, so the instantiations below get ordinary default visibility and
 * can actually be exported.
 */
#define _LIBCPP_DISABLE_VISIBILITY_ANNOTATIONS

#include <algorithm>
#include <cmath>
#include <string>

/* noinline + a runtime-only length parameter (n): the compiler compiles
 * these in complete isolation, with no visibility into what the (never
 * actually executed) caller passes, so it cannot fold away sort's
 * small-size insertion-sort shortcut or partial_sort/heap's algorithms -
 * they get fully instantiated regardless of n. */
/* _ClassicAlgPolicy combined with std::ranges::less as the comparator is
 * neither plain std::sort/partial_sort/... (which use __less<>) nor
 * std::ranges::sort/... (which use _RangeAlgPolicy) - it is the classic,
 * non-ranges iterator-pair overloads called with an explicit
 * std::ranges::less{} comparator object. std::sort itself is a dead end
 * for the introsort family specifically: libc++ has a fast-path overload
 * of __sort_dispatch(_Type*, _Type*, ranges::less&) for the common
 * arithmetic types (int, unsigned long, ...) that redirects to a
 * library-provided __sort<__less<_Type>&, _Type*> instead of locally
 * instantiating __introsort/__insertion_sort/__bitset_partition/__sort3-5
 * - so calling std::sort(..., ranges::less{}) never generates them. Call
 * std::__introsort<...> directly (bypassing std::sort's public overload
 * set entirely) to force exactly the instantiation libjxl_1 needs. */
__attribute__((noinline))
static void force_sort_int(int *data, unsigned long n) {
    std::ranges::less comp{};
    std::__introsort<std::_ClassicAlgPolicy, std::ranges::less, int *, true>(data, data + n, comp, (long)n);
    std::partial_sort(data, data + n / 2, data + n, std::ranges::less{});
    std::make_heap(data, data + n, std::ranges::less{});
    std::pop_heap(data, data + n, std::ranges::less{});
    std::sort_heap(data, data + n - 1, std::ranges::less{});
    std::push_heap(data, data + n, std::ranges::less{});
    std::sort(data, data + n); /* library fast-path: __sort<__less<int>&, int*> */
}

__attribute__((noinline))
static void force_sort_ulong(unsigned long *data, unsigned long n) {
    std::ranges::less comp{};
    std::__introsort<std::_ClassicAlgPolicy, std::ranges::less, unsigned long *, true>(data, data + n, comp, (long)n);
    std::partial_sort(data, data + n / 2, data + n, std::ranges::less{});
    std::make_heap(data, data + n, std::ranges::less{});
    std::pop_heap(data, data + n, std::ranges::less{});
    std::sort_heap(data, data + n - 1, std::ranges::less{});
    std::push_heap(data, data + n, std::ranges::less{});
    std::sort(data, data + n); /* library fast-path: __sort<__less<unsigned long>&, unsigned long*> */
}

__attribute__((noinline))
static void force_string_ops(const char *src, unsigned long n) {
    std::string s;
    for (unsigned long i = 0; i < n; i++) {
        s.push_back(src[i % 26]);
        s.append(src, i % 13);
        s.append(src);
        s += src[i % 26];
    }
    s.resize(n * 3, 'x');
    s.reserve(n * 10);
    s.erase(s.begin() + 1, s.begin() + 3);
    s.erase(1);
    s.erase(2, 3);

    /* short (SSO) <-> long (external) transitions, both directions, to
     * force __assign_no_alias<true>/<false>, __reset_internal_buffer,
     * __grow_by_and_replace, __grow_by_without_replace, __init variants. */
    std::string s2(s.c_str());
    s2.assign(s.c_str());
    s2.assign(s.c_str(), s.size());
    s2.assign("short");
    s2 = "short2";
    s2.assign(s.c_str(), s.size());
    s2.resize(2);
    s2.resize(n * 5, 'y');
    s2.shrink_to_fit();
    s2.insert(0, src, 5);
    s2.replace(0, 2, src, 3);

    std::string s3 = s.substr(1, 5);
    std::string s4(s, 1, 5);
    s4.reserve(n * 20);

    /* copy ctor / assignment from an already-external (long) string. */
    std::string s5(s2);
    std::string s6 = s;
    s6 = s2;

    /* large allocation to trigger __libcpp_aligned_alloc's path. */
    std::string s7(n * 1000, 'z');

    static std::string keep_alive = s + s2 + s3 + s4 + s5 + s6 + s7; /* nontrivial static dtor -> __cxa_atexit */
    (void)keep_alive;
}

extern "C" void __jxl_libcxx_force_instantiate(void);

__attribute__((constructor))
static void __jxl_libcxx_force_ctor(void) {
    __jxl_libcxx_force_instantiate();
}

void __jxl_libcxx_force_instantiate(void) {
    static volatile bool never = false;
    if (!never) return;

    int ints[128];
    for (int i = 0; i < 128; i++) ints[i] = 128 - i;
    force_sort_int(ints, 128);

    unsigned long ulongs[128];
    for (int i = 0; i < 128; i++) ulongs[i] = 128 - i;
    force_sort_ulong(ulongs, 128);

    force_string_ops("a long enough string to defeat SSO for sure", 40);

    int *arr = new int[4];
    delete[] arr;

    volatile double d = sqrt(2.0);
    volatile float f1 = sqrtf(2.0f);
    volatile float f2 = cbrtf(2.0f);
    volatile float f3 = hypotf(1.0f, 2.0f);
    volatile long l = llroundf(2.5f);
    (void)d; (void)f1; (void)f2; (void)f3; (void)l;
}
