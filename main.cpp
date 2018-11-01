#include <unordered_set>
#include <unordered_map>
#include <list>
#include <map>
#include <iomanip>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <algorithm>
#include <stack>

#include <cstdlib>
#include <ctime>

using item_t = char;
using input_t = std::string;

constexpr item_t ZERO = '\0';

struct State
{
	constexpr static int ORDER = 3;

	struct Hash
	{
		std::size_t operator()(const State& state) const noexcept;
	};

	State();

	char operator()(const std::size_t n) const { return m_state[ORDER - n - 1]; }
	bool operator==(const State& other) const;

	std::ostream& dump(std::ostream& os) const;

	item_t push_back(const item_t& item);
	item_t push_front(const item_t& item);

	void clear();

private:
	item_t m_state[ORDER];
};

std::ostream& operator<<(std::ostream& os, const State& state)
{
	return state.dump(os);
}

std::size_t State::Hash::operator()(const State& state) const noexcept
{
#if defined(_WIN64)
	static_assert(sizeof(std::size_t) == 8, "This code is for 64-bit size_t.");
	const std::size_t _FNV_offset_basis = 14695981039346656037ULL;
	const std::size_t _FNV_prime = 1099511628211ULL;

#else /* defined(_WIN64) */
	static_assert(sizeof(std::size_t) == 4, "This code is for 32-bit size_t.");
	const std::size_t _FNV_offset_basis = 2166136261U;
	const std::size_t _FNV_prime = 16777619U;
#endif /* defined(_WIN64) */

	std::size_t _Val = _FNV_offset_basis;
	for (std::size_t _Next = 0; _Next != State::ORDER; ++_Next)
	{	// fold in another byte
		_Val ^= (std::size_t)state.m_state[_Next];
		_Val *= _FNV_prime;
	}

	return _Val;
}

State::State()
{
	clear();
}

bool State::operator==(const State& other) const
{
	for (std::size_t i = 0; i != ORDER; ++i)
	{
		if (m_state[i] != other.m_state[i])
		{
			return false;
		}
	}

	return true;
}

std::ostream& State::dump(std::ostream& os) const
{
	for (const auto& i : m_state)
	{
		os << std::hex << " 0x" << std::setw(2) << std::setfill('0')
			<< static_cast<unsigned>(static_cast<unsigned char>(i));
	}

	return os;
}

item_t State::push_back(const item_t& item)
{
	const item_t result = m_state[0];

	for (std::size_t i = 0; 1 + i != ORDER; ++i)
	{
		m_state[i] = m_state[1 + i];
	}

	m_state[ORDER - 1] = item;

	return result;
}

item_t State::push_front(const item_t& item)
{
	const item_t result = m_state[ORDER - 1];

	for (std::size_t i = ORDER - 1; i != 0; --i)
	{
		m_state[i] = m_state[i - 1];
	}

	m_state[0] = item;

	return result;
}

void State::clear()
{
	for (std::size_t i = 0; i != ORDER; ++i)
	{
		m_state[i] = ZERO;
	}
}

class MarkovChain
{
public:
	class Transition
	{
	public:
		using frequency_t = std::pair<item_t, int>;
		using frequencies_t = std::vector<frequency_t>;

		Transition() {}

		void add(const item_t& item);
		auto count() const { return m_item_to_frequency.size(); }

		bool operator==(const Transition& rhv) const;

		frequencies_t::const_iterator begin() const;
		frequencies_t::const_iterator end() const;

		void refine(const int threshold);
		const item_t& get(const std::size_t index) const;

	private:
		void build_frequencies() const;

		std::unordered_map<item_t, int> m_item_to_frequency;
		mutable frequencies_t m_frequencies;
	};

	using transition_t = Transition;

	class Iterator
	{
	public:
		Iterator(const MarkovChain& m_chain);
		Iterator(const MarkovChain& chain, const int min, const int max);

		void operator++();
		bool operator==(const Iterator& rhv) const { return m_position == rhv.m_position; }
		bool operator!=(const Iterator& rhv) const { return !(*this == rhv); }

		std::string operator*() const;

	private:
		using lever_t = std::pair<const transition_t&, transition_t::frequencies_t::const_iterator>;

		void roll();
		void dive();

		const MarkovChain& m_chain;

		std::size_t m_min;
		std::size_t m_max;

		std::stack<item_t> m_popped;
		State m_state;
		std::list<lever_t> m_position;
	};

	template<typename T>
	MarkovChain(const T& iterable, const bool add_zero = true);

	void random();

	Iterator begin(int min, int max) const { return Iterator(*this, min, max); }
	Iterator end() const { return Iterator(*this); }

private:
	void add_state(const State& state, const item_t& i);
	void add_input(const input_t& input, const bool add_zero);

	void iteration(std::string& result) const;

	const transition_t& transition(const State& state) const { return m_chain.at(state); }

protected:
	std::unordered_map<State, transition_t, State::Hash> m_chain;
};

class NumberEncoder : public MarkovChain
{
public:
	template<typename T>
	NumberEncoder(const T& words, const int threshold = 15) : MarkovChain(words, false)
	{
		refine(threshold);
	}

	bool encode(int number, std::string& result);

private:
	void refine(const int threshold);
};

bool NumberEncoder::encode(int number, std::string& result)
{
	std::stringstream ss;

	int offset = 0;
	State state;
	while (0 < number)
	{
		auto transition_i = m_chain.find(state);

		if (m_chain.end() == transition_i)
		{
			std::cerr << "Logic error: didn't find transition for the state " << state << std::endl;
			return false;
		}

		if (0 == transition_i->second.count())
		{
			state.clear();
			transition_i = m_chain.find(state);
		}

		const auto transition = transition_i->second;
		const auto index = (number + offset) % transition.count();
		ss << transition.get(index);
		number /= static_cast<int>(transition.count());
		++offset;
	}

	result = ss.str();

	return true;
}

void NumberEncoder::refine(const int threshold)
{
	for (auto& t : m_chain)
	{
		t.second.refine(threshold);
	}
}

MarkovChain::Iterator::Iterator(const MarkovChain& chain, const int min, const int max) :
	m_chain(chain),
	m_min(min),
	m_max(max)
{
	const transition_t& transition = m_chain.transition(m_state);
	m_position.emplace_front(transition, transition.begin());

	dive();

	if (m_position.size() < m_min
		|| m_position.front().second->first != ZERO)
	{
		++*this;
	}
}

MarkovChain::Iterator::Iterator(const MarkovChain& chain):
	m_chain(chain),
	m_min(0),
	m_max(0)
{
	// corresponds to the end()
}

void MarkovChain::Iterator::operator++()
{
	do
	{
		roll();
		dive();
	} while (!m_position.empty()
		&& (m_position.size() < m_min
			|| m_position.front().second->first != ZERO));
}

std::string MarkovChain::Iterator::operator*() const
{
	std::stringstream result;
	for (auto p_i = m_position.rbegin(); p_i != m_position.rend(); ++p_i)
	{
		if (ZERO != p_i->second->first)
		{
			result << p_i->second->first;
		}
	}
	return result.str();
}

void MarkovChain::Iterator::roll()
{
	if (m_position.empty())
	{
		return;	// at the end
	}

	auto last = &m_position.front();
	do
	{
		while (last->second == last->first.end()
			&& !m_position.empty())
		{
			m_position.pop_front();
			if (m_position.empty())
			{
				return;
			}

			last = &m_position.front();

			// get back to previous state
			m_state.push_front(m_popped.top());
			m_popped.pop();
		}

		if (m_position.empty())
		{
			return;
		}

		++last->second;
	} while (last->second == last->first.end());
}

void MarkovChain::Iterator::dive()
{
	if (m_position.empty())
	{
		return;
	}

	auto last = &m_position.front();
	if (last->second == last->first.end())
	{
		throw std::logic_error("dive without roll.");
	}

	while (last->second->first != ZERO
		&& m_position.size() < m_max)
	{
		m_popped.push(m_state.push_back(last->second->first));

		const transition_t& transition = m_chain.transition(m_state);
		m_position.emplace_front(transition, transition.begin());
		last = &m_position.front();
	}
}

void MarkovChain::Transition::add(const item_t& item)
{
	auto transition_i = m_item_to_frequency.find(item);
	if (m_item_to_frequency.end() == transition_i)
	{
		m_item_to_frequency[item] = 1;
	}
	else
	{
		++transition_i->second;
	}

	m_frequencies.clear();
}

bool MarkovChain::Transition::operator==(const Transition& rhv) const
{
	return m_item_to_frequency == rhv.m_item_to_frequency;
}

MarkovChain::Transition::frequencies_t::const_iterator MarkovChain::Transition::begin() const
{
	if (0 == m_frequencies.size())
	{
		build_frequencies();
	}

	return m_frequencies.begin();
}

MarkovChain::Transition::frequencies_t::const_iterator MarkovChain::Transition::end() const
{
	if (0 == m_frequencies.size())
	{
		build_frequencies();
	}

	return m_frequencies.end();
}

void MarkovChain::Transition::refine(const int threshold)
{
	build_frequencies();

	for (auto i = threshold; i < m_frequencies.size(); ++i)
	{
		m_item_to_frequency.erase(m_frequencies[i].first);
	}

	m_frequencies.clear();
}

const item_t& MarkovChain::Transition::get(const std::size_t index) const
{
	build_frequencies();

	return m_frequencies[index].first;
}

void MarkovChain::Transition::build_frequencies() const
{
	if (!m_frequencies.empty())
	{
		return;
	}

	m_frequencies.reserve(m_item_to_frequency.size());
	for (const auto& i : m_item_to_frequency)
	{
		m_frequencies.push_back(i);
	}

	std::sort(m_frequencies.begin(), m_frequencies.end(),
		[](const auto& a, const auto& b) { return a.second > b.second; });
}

template<typename T>
MarkovChain::MarkovChain(const T& iterable, const bool add_zero)
{
	for (auto i = iterable.begin(); i != iterable.end(); ++i)
	{
		const input_t input = *i;
		add_input(input, add_zero);
	}
}

void MarkovChain::random()
{
	std::string result;

	iteration(result);

	std::cout << result << std::endl;
}

void MarkovChain::add_state(const State& state, const item_t& i)
{
	const auto state_i = m_chain.find(state);
	if (m_chain.end() == state_i)
	{
		transition_t transition;
		transition.add(i);
		m_chain.emplace(state, transition);

		return;
	}

	transition_t& transition = state_i->second;
	transition.add(i);
}

void MarkovChain::add_input(const input_t& input, const bool add_zero)
{
	State state;
	for (std::size_t i = 0; input[i] != ZERO; ++i)
	{
		add_state(state, input[i]);

		state.push_back(input[i]);
	}

	if (add_zero)
	{
		add_state(state, ZERO);
	}
}

void MarkovChain::iteration(std::string& result) const
{
	State state;
	do
	{
		const auto i = m_chain.find(state);
		if (i == m_chain.end())
		{
			throw std::logic_error("No transition.");
		}

		int count = 0;
		std::list<std::pair<item_t, int>> states;
		for (const auto& t : i->second)
		{
			states.push_back(t);
			count += t.second;
		}

		bool found = false;
		const int choose = std::rand() % count;
		int current_count = 0;;
		for (const auto& s : states)
		{
			current_count += s.second;
			if (choose < current_count)
			{
				if (ZERO != s.first)
				{
					result.push_back(s.first);
				}

				state.push_back(s.first);
				found = true;
				break;
			}
		}

		if (!found)
		{
			throw std::logic_error("Didn't find transition.");
		}
	} while (ZERO != state(0));
}

using samples_t = std::list<std::string>;
bool read_file(const char* file_name, samples_t& names)
{
	std::ifstream ifs(file_name);
	if (!ifs.is_open())
	{
		return false;
	}

	std::string line;
	while (ifs >> line)
	{
		names.push_back(line);
	}

	return true;
}

void random(MarkovChain& chain)
{
	std::srand(static_cast<unsigned>(time(nullptr)));
	chain.random();
}

void enumerate(MarkovChain& chain, const samples_t& samples)
{
	std::unordered_set<std::string> used;
	for (const auto& s : samples)
	{
		used.insert(s);
	}

	int count = 0;
	auto i = chain.begin(5, 10);
	while (i != chain.end())
	{
		const std::string name = *i;

		if (used.find(name) == used.end())
		{
			std::cout << name << std::endl;
		}
		++i;
	}
}

enum Mode
{
	RANDOM,
	ALL
};

int generate(const samples_t& samples, const Mode mode)
{
	MarkovChain chain(samples);

	switch (mode)
	{
	case RANDOM:
		random(chain);
		break;

	case ALL:
		enumerate(chain, samples);
		break;
	}

	return 0;
}

using int_list_t = std::list<int>;
int encode(const samples_t& samples, const int_list_t& numbers)
{
	NumberEncoder encoder(samples);

	std::string result;
	for (const auto& number : numbers)
	{
		unsigned value = number;
		for (int i = 0; i != 8; ++i)
		{
			value ^= 0b10011010;
			value = ((value << 1) & (~0u >> 1)) | (value >> (8*sizeof(value) - 2));
		}

		encoder.encode(value, result);
		std::cout << "Number " << number << ": '" << result << "'" << std::endl;
	}

	return 0;
}

int main(int argc, char** argv)
{
	Mode mode = ALL;

	if (2 > argc)
	{
		std::cerr << "Usage: " << argv[0] << " <name of the file with samples> [-r | -e <number>]" << std::endl;
		return 1;
	}

	samples_t samples;
	if (!read_file(argv[1], samples))
	{
		perror("couldn't read from file");
		return 1;
	}

	if (3 == argc && 0 == strcmp("-r", argv[2]))
	{
		mode = RANDOM;
	}
	
	if (4 == argc && 0 == strcmp("-e", argv[2]))
	{
		constexpr auto delimiter = ',';

		int_list_t numbers;
		const auto dot_pos = std::strchr(argv[3], delimiter);
		if (nullptr == dot_pos)
		{
			const auto number = static_cast<int>(std::strtoul(argv[3], nullptr, 10));
			numbers.push_back(std::abs(number));
		}
		else
		{
			std::string number_str;
			std::istringstream stream(argv[3]);
			while (std::getline(stream, number_str, delimiter))
			{
				const auto number = static_cast<int>(std::strtoul(number_str.c_str(), nullptr, 10));
				numbers.push_back(number);
			}
		}

		return encode(samples, numbers);
	}

	return generate(samples, mode);
}
