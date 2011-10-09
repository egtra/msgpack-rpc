#ifndef MP_CSTDINT_H__
#define MP_CSTDINT_H__

#ifdef MP_CSTDINT_BOOST_ORG
#include <boost/cstdint.hpp>

namespace mp {
	using boost::int8_t;
	using boost::int16_t;
	using boost::int32_t;
	using boost::int64_t;
	using boost::uint8_t;
	using boost::uint16_t;
	using boost::uint32_t;
	using boost::uint64_t;

	using boost::int_least8_t;
	using boost::int_least16_t;
	using boost::int_least32_t;
	using boost::int_least64_t;
	using boost::uint_least8_t;
	using boost::uint_least16_t;
	using boost::uint_least32_t;
	using boost::uint_least64_t;

	using boost::int_fast8_t;
	using boost::int_fast16_t;
	using boost::int_fast32_t;
	using boost::int_fast64_t;
	using boost::uint_fast8_t;
	using boost::uint_fast16_t;
	using boost::uint_fast32_t;
	using boost::uint_fast64_t;

	using boost::intmax_t;
	using boost::uintmax_t;
}

#if _MSC_VER >= 1400
#include <stdlib.h>
namespace mp {
	using ::intptr_t;
	using ::uintptr_t;
}
#elif defined _WIN32
#include <windows.h>
namespace mp {
	typedef ::INT_PTR intptr_t;
	typedef ::UINT_PTR uintptr_t;
}
#else
#include <stdint.h>
namespace mp {
	using ::intptr_t;
	using ::uintptr_t;
}
#endif

#else
#ifdef MP_CSTDINT_STANDARD
#include <cstdint>
namespace mp {
	using std::int8_t;
	using std::int16_t;
	using std::int32_t;
	using std::int64_t;
	using std::uint8_t;
	using std::uint16_t;
	using std::uint32_t;
	using std::uint64_t;

	using std::int_least8_t;
	using std::int_least16_t;
	using std::int_least32_t;
	using std::int_least64_t;
	using std::uint_least8_t;
	using std::uint_least16_t;
	using std::uint_least32_t;
	using std::uint_least64_t;

	using std::int_fast8_t;
	using std::int_fast16_t;
	using std::int_fast32_t;
	using std::int_fast64_t;
	using std::uint_fast8_t;
	using std::uint_fast16_t;
	using std::uint_fast32_t;
	using std::uint_fast64_t;

	using std::intmax_t;
	using std::intptr_t;
	using std::uintmax_t;
	using std::uintptr_t;

}
#else
#error You must define MP_CSTDINT_STANDARD or MP_CSTDINT_STANDARD
#endif
#endif

#endif /* mp/functional.h */