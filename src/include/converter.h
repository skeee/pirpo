#pragma once

#include <string>
#include <memory>
#include <tuple>
#include <optional>



// Basic conversion class.
struct conversion { virtual ~conversion() = default; };

// Linear conversion class.
// Almost all physical metrics can be converted using linear transform.
class linear : public conversion
{
public:
	linear() = delete;

	linear(float factor = 1.f, float offset = 0.f)
	{
		constexpr static float EPSILON = 1e-10f;

		if (std::abs(factor) < EPSILON)
			throw "'factor' argument is too small!";

		m_factor = factor;
		m_divider = 1.f / factor;
		m_offset = offset;
	}

	// Convert from basic value to derivative one.
	float forward(float value) const
	{
		return m_factor * value + m_offset;
	}

	// Convert from derivative value to basic one.
	float backward(float value) const
	{
		return (value - m_offset) * m_divider;
	}

private:
	float m_factor;
	float m_divider;
	float m_offset;
};


// Metrics.
// Weight metrics.
struct gramm { constexpr static char const * signature = "g"; };
struct lb { constexpr static char const * signature = "lb"; };
struct pood { constexpr static char const * signature = "p"; };
using weight_metrics = std::tuple<gramm, lb, pood>;

// Distance metrics.
struct meter { constexpr static char const * signature = "m"; };
struct mile { constexpr static char const * signature = "ml"; };
struct verst { constexpr static char const * signature = "v"; };
using distance_metrics = std::tuple<meter, mile, verst>;

// Temperature metrics.
struct celsius { constexpr static char const * signature = "c"; };
struct fahrenheit { constexpr static char const * signature = "f"; };
struct kelvin { constexpr static char const * signature = "k"; };
using temperature_metrics = std::tuple<celsius, fahrenheit, kelvin>;

// Possible conversion types.
template<class _primary, class _minor>
struct metric_conversion_type;

// Weight.
template<> struct metric_conversion_type<gramm, lb> : public linear { metric_conversion_type() : linear(453.592f) { ; } };
template<> struct metric_conversion_type<gramm, pood> : public linear { metric_conversion_type() : linear(16'380.7f) { ; } };

// Distance.
template<> struct metric_conversion_type<meter, mile> : public linear { metric_conversion_type() : linear(1'609.34f) { ; } };
template<> struct metric_conversion_type<meter, verst> : public linear { metric_conversion_type() : linear(1'066.8f) { ; } };

// Temperature.
template<> struct metric_conversion_type<celsius, fahrenheit> : public linear
{
	metric_conversion_type() : linear(9.f / 5.f, 32.f) { ; }

	// Convert from basic value to derivative one.
	float forward(float value) const
	{
		return linear::backward(value);
	}

	// Convert from derivative value to basic one.
	float backward(float value) const
	{
		return linear::forward(value);
	}
};
template<> struct metric_conversion_type<celsius, kelvin> : public linear { metric_conversion_type() : linear(1.f, 273.15f) { ; } };


// Conversion factory.
class converter_factory
{
public:
	static const std::unique_ptr<converter_factory> & instance()
	{
		if (!m_instance)
			m_instance = std::unique_ptr<converter_factory>(new converter_factory);
		return m_instance;
	}

	template<class _primary, class _minor>
	const auto & converter()
	{
		thread_local metric_conversion_type<_primary, _minor> conversion{};
		return conversion;
	}

protected:
	inline static std::unique_ptr<converter_factory> m_instance = nullptr;

	converter_factory() = default;
};


// Converters.
// Primary/minor.
template<class _primary, class _minor, bool _direction>
struct primary_metrics_converter
{
	constexpr static char const * from_signature =
		std::conditional<!_direction, _primary, _minor>::type::signature;
	constexpr static char const * to_signature =
		std::conditional<_direction, _primary, _minor>::type::signature;

	float convert(float value) const
	{
		if constexpr (std::is_same<_primary, _minor>::value)
			return value;
		else if constexpr (_direction)
			return converter_factory::instance()->converter<_primary, _minor>().forward(value);
		else
			return converter_factory::instance()->converter<_primary, _minor>().backward(value);
	}
};

// Minor/minor.
template<class _primary, class _first_minor, class _second_minor>
struct minor_metrics_converter
{
	constexpr static char const * from_signature = _second_minor::signature;
	constexpr static char const * to_signature = _first_minor::signature;

	float convert(float value) const
	{
		if constexpr (std::is_same<_first_minor, _second_minor>::value)
			return value;
		else
		{
			const auto & factory = converter_factory::instance();
			return factory->converter<_primary, _second_minor>().forward(
				factory->converter<_primary, _first_minor>().backward(value)
			);
		}
	}
};

// Responsible actors.
// Basic responsible.
class responsible
{
public:
	//responsible() = default;
	virtual ~responsible() = default;

	void add_next(std::unique_ptr<responsible> & next)
	{
		m_next = std::move(next);
	}

	virtual std::optional<float> process(const std::string & from, const std::string & to, float value) = 0;

protected:
	std::unique_ptr<responsible> m_next;
};

// Responsible implementation.
template<class _metrics_converter>
class responsible_impl : public responsible
{
public:
	//responsible_impl() = default;
	std::optional<float> process(const std::string & from, const std::string & to, float value)
	{
		if (is_reponsible(from, to))
			return m_converter.convert(value);
		else if (m_next)
			return m_next->process(from, to, value);
		else
			return std::nullopt;
	}

	bool is_reponsible(const std::string & from, const std::string & to) const
	{
		return (_metrics_converter::from_signature == from) &&
			(_metrics_converter::to_signature == to);
	}


protected:
	_metrics_converter m_converter;
};


// Converter traits.
// tuple<tuple<conversion_type, ...>, ...> to tuple<conversion, ...>
template<class...>
struct converter_traits;

template<class _primary, class... _minors>
struct converter_traits<std::tuple<_primary, _minors...>>
{
	template<class _first_minor, class... _other_minors>
	struct combined
	{
		using type = std::tuple<minor_metrics_converter<_primary, _first_minor, _other_minors>...>;
	};

	using primary_minor_conversions = std::tuple<primary_metrics_converter<_primary, _minors, true>...>;
	using minor_primary_conversions = std::tuple<primary_metrics_converter<_primary, _minors, false>...>;
	using minor_conversions = decltype(std::tuple_cat(std::declval<typename combined<_minors, _minors...>::type>()...));
	using type = decltype(std::tuple_cat(
		std::declval<primary_minor_conversions>(),
		std::declval<minor_primary_conversions>(),
		std::declval<minor_conversions>()));
};


// Converter.
template<class... _metrics>
class converter
{
public:
	static const std::unique_ptr<converter> & instance()
	{
		if (!m_instance)
			m_instance = std::unique_ptr<converter>(new converter);
		return m_instance;
	}

	std::optional<float> process(const std::string & from, const std::string & to, float value)
	{
		if (m_responsible)
			return m_responsible->process(from, to, value);
		return std::nullopt;
	}


protected:
	converter()
	{
		add_responsible_groups<_metrics...>();
	}

	// Add group of responsible actors.
	template<class _first, class... _rest>
	void add_responsible_groups()
	{
		add_responsibles<typename converter_traits<_first>::type>();
		if constexpr (sizeof...(_rest) != 0)
			add_responsible_groups<_rest...>();
	}

	// Split tuple into two parts.
	template<class...>
	struct tuple_splitter;
	template<class _first, class... _rest>
	struct tuple_splitter<std::tuple<_first, _rest...>>
	{
		using first_type = _first;
		using other_types = std::tuple<_rest...>;
	};

	// Add responsible actors form one metric group (for example wight or distance).
	template<class _converters>
	void add_responsibles()
	{
		using splitted = tuple_splitter<_converters>;
		auto reponsible_actor = std::make_unique<responsible_impl<typename splitted::first_type>>();
		reponsible_actor->add_next(m_responsible);
		m_responsible = std::move(reponsible_actor);

		if constexpr (std::tuple_size<typename splitted::other_types>::value != 0)
		{
			add_responsibles<typename splitted::other_types>();
		}
	}

	inline static std::unique_ptr<converter> m_instance = nullptr;
	std::unique_ptr<responsible> m_responsible;
};
