// This file is distributed under the MIT license.
// See the LICENSE file for details.

#include <cstddef>
#include <iterator>
#include <utility>
#include <type_traits>

#include <visionaray/bvh.h>
#include <visionaray/intersector.h>
#include <visionaray/update_if.h>

#include "exit_traversal.h"
#include "macros.h"
#include "multi_hit.h"

namespace visionaray
{
namespace detail
{

//-------------------------------------------------------------------------------------------------
// Traverse any primitives but BVHs
//

template <
    traversal_type Traversal,
    size_t MultiHitMax = 1,             // Max hits for multi-hit traversal
    typename Cond,
    typename R,
    typename P,
    typename Intersector
    >
VSNRAY_FUNC
inline auto traverse(
        std::false_type                 /* is no bvh */,
        Cond                            update_cond,
        R const&                        r,
        P                               begin,
        P                               end,
        typename R::scalar_type const&  max_t,
        Intersector&                    isect
        )
    -> typename traversal_result<decltype( isect(r, *begin) ), Traversal, MultiHitMax>::type
{
    using RT = typename traversal_result<decltype( isect(r, *begin) ), Traversal, MultiHitMax>::type;

    RT result;

    for (P it = begin; it != end; ++it)
    {
        auto hr = isect(r, *it);
        update_if(result, hr, update_cond(hr, result, max_t));

        exit_traversal<Traversal> early_exit;
        if (early_exit.check(result))
        {
            return result;
        }
    }

    return result;
}


//-------------------------------------------------------------------------------------------------
// Traverse BVHs. Those have a special intersect method that takes the traversal
// type as an additional parameter.
//

template <
    traversal_type Traversal,
    size_t MultiHitMax = 1,             // Max hits for multi-hit traversal
    typename Cond,
    typename R,
    typename P,
    typename Intersector
    >
VSNRAY_FUNC
inline auto traverse(
        std::true_type                  /* is_bvh */,
        Cond                            update_cond,
        R const&                        r,
        P                               begin,
        P                               end,
        typename R::scalar_type const&  max_t,
        Intersector&                    isect
        )
    -> decltype( isect(
            std::integral_constant<int, Traversal>{},
            std::integral_constant<size_t, MultiHitMax>{},
            r,
            *begin,
            max_t,
            update_cond
            ) )
{
    using RT = decltype( isect(
            std::integral_constant<int, Traversal>{},
            std::integral_constant<size_t, MultiHitMax>{},
            r,
            *begin,
            max_t,
            update_cond
            ) );

    RT result;

    for (P it = begin; it != end; ++it)
    {
        auto hr = isect(
                std::integral_constant<int, Traversal>{},
                std::integral_constant<size_t, MultiHitMax>{},
                r,
                *it,
                max_t,
                update_cond
                );

        update_if(result, hr, update_cond(hr, result, max_t));

        exit_traversal<Traversal> early_exit;
        if (early_exit.check(result))
        {
            return result;
        }
    }

    return result;
}

template <
    traversal_type Traversal,
    size_t MultiHitMax = 1,
    typename IsAnyBVH,
    typename Cond,
    typename R,
    typename Primitives,
    typename Intersector
    >
VSNRAY_FUNC
inline auto traverse(
        IsAnyBVH        /* */,
        Cond            update_cond,
        R const&        r,
        Primitives      begin,
        Primitives      end,
        Intersector&    isect
        )
    -> decltype( traverse<Traversal, MultiHitMax>(
            IsAnyBVH{},
            update_cond,
            r,
            begin,
            end,
            numeric_limits<float>::max(),
            isect
            ) )
{
    return traverse<Traversal, MultiHitMax>(
            IsAnyBVH{},
            update_cond,
            r,
            begin,
            end,
            numeric_limits<float>::max(),
            isect
            );
}

} // detail



//-------------------------------------------------------------------------------------------------
// any hit w/o max_t
//

template <
    typename R,
    typename Primitives,
    typename Intersector,
    typename Primitive = typename std::iterator_traits<Primitives>::value_type
    >
VSNRAY_FUNC
inline auto any_hit(
        R const&        r,
        Primitive       begin,
        Primitive       end,
        Intersector&    isect
        )
    -> decltype( detail::traverse<detail::AnyHit>(
            std::integral_constant<bool, is_any_bvh<Primitive>::value>{},
            is_closer_t(),
            r,
            begin,
            end,
            isect
            ) )
{
    return detail::traverse<detail::AnyHit>(
            std::integral_constant<bool, is_any_bvh<Primitive>::value>{},
            is_closer_t(),
            r,
            begin,
            end,
            isect
            );
}

template <typename R, typename P>
VSNRAY_FUNC
inline auto any_hit(R const& r, P begin, P end)
    -> decltype( intersect(r, *begin) )
{
    default_intersector ignore;
    return any_hit(r, begin, end, ignore);
}


//-------------------------------------------------------------------------------------------------
// any hit with max_t
//

template <
    typename R,
    typename Primitives,
    typename Intersector,
    typename Primitive = typename std::iterator_traits<Primitives>::value_type
    >
VSNRAY_FUNC
inline auto any_hit(
        R const&                        r,
        Primitives                      begin,
        Primitives                      end,
        typename R::scalar_type const&  max_t,
        Intersector&                    isect
        )
    -> decltype( detail::traverse<detail::AnyHit>(
            std::integral_constant<bool, is_any_bvh<Primitive>::value>{},
            is_closer_t(),
            r,
            begin,
            end,
            max_t,
            isect
            ) )
{
    return detail::traverse<detail::AnyHit>(
            std::integral_constant<bool, is_any_bvh<Primitive>::value>{},
            is_closer_t(),
            r,
            begin,
            end,
            max_t,
            isect
            );
}

template <typename R, typename Primitives>
VSNRAY_FUNC
inline auto any_hit(
        R const&                        r,
        Primitives                      begin,
        Primitives                      end,
        typename R::scalar_type const&  max_t
        )
    -> decltype( any_hit(r, begin, end, max_t, std::declval<default_intersector&>() ) )
{
    default_intersector ignore;
    return any_hit(r, begin, end, max_t, ignore);
}


//-------------------------------------------------------------------------------------------------
// closest hit 
//

template <
    typename R,
    typename Primitives,
    typename Intersector,
    typename Primitive = typename std::iterator_traits<Primitives>::value_type
    >
VSNRAY_FUNC
inline auto closest_hit(
        R const&        r,
        Primitives      begin,
        Primitives      end,
        Intersector&    isect
        )
    -> decltype( detail::traverse<detail::ClosestHit>(
            std::integral_constant<bool, is_any_bvh<Primitive>::value>{},
            is_closer_t(),
            r,
            begin,
            end,
            isect
            ) )
{
    return detail::traverse<detail::ClosestHit>(
            std::integral_constant<bool, is_any_bvh<Primitive>::value>{},
            is_closer_t(),
            r,
            begin,
            end,
            isect
            );
}

template <typename R, typename Primitives>
VSNRAY_FUNC
inline auto closest_hit(R const& r, Primitives begin, Primitives end)
    -> decltype( closest_hit(r, begin, end, std::declval<default_intersector&>()) )
{
    default_intersector ignore;
    return closest_hit(r, begin, end, ignore);
}


//-------------------------------------------------------------------------------------------------
// multi hit
//

template <
    size_t   N = 16,
    typename R,
    typename Primitives,
    typename Intersector,
    typename Primitive = typename std::iterator_traits<Primitives>::value_type
    >
VSNRAY_FUNC
inline auto multi_hit(
        R const&        r,
        Primitives      begin,
        Primitives      end,
        Intersector&    isect
        )
    -> decltype( detail::traverse<detail::MultiHit, N>(
            std::integral_constant<bool, is_any_bvh<Primitive>::value>{},
            is_closer_t(),
            r,
            begin,
            end,
            isect
            ) )
{
    return detail::traverse<detail::MultiHit, N>(
            std::integral_constant<bool, is_any_bvh<Primitive>::value>{},
            is_closer_t(),
            r,
            begin,
            end,
            isect
            );
}

template <
    size_t N = 16,
    typename R,
    typename Primitives,
    typename Primitive = typename std::iterator_traits<Primitives>::value_type
    >
VSNRAY_FUNC
inline auto multi_hit(R const& r, Primitives begin, Primitives end)
    // Workaround gcc-4.8.1 ICE
    // The following FAILS with lots of recursive template instantiations!
//  -> decltype( multi_hit<N>(r, begin, end, std::declval<default_intersector&>()) )
    -> decltype( detail::traverse<detail::MultiHit, N>(
            std::integral_constant<bool, is_any_bvh<Primitive>::value>{},
            is_closer_t(),
            r,
            begin,
            end,
            std::declval<default_intersector&>()
            ) )
{
    default_intersector ignore;
    return multi_hit<N>(r, begin, end, ignore);
}

} // visionaray
