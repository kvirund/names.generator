#include <unordered_set>
#include <unordered_map>
#include <list>
#include <map>
#include <iostream>
#include <fstream>
#include <string>

#include <cstdlib>
#include <ctime>

using item_t = char;
using input_t = std::string;

constexpr item_t ZERO = '\0';

struct State
{
	constexpr static int ORDER = 1;

	struct Hash
	{
		std::size_t operator()(const State& state) const noexcept;
	};

	State();

	char operator()(const std::size_t n) const { return m_state[ORDER - n - 1]; }
	bool operator==(const State& other) const;

	void push(const item_t& item);

private:
	item_t m_state[ORDER];
};

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
	for (std::size_t i = 0; i != ORDER; ++i)
	{
		m_state[i] = ZERO;
	}
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

void State::push(const item_t& item)
{
	for (std::size_t i = 0; 1 + i != ORDER; ++i)
	{
		m_state[i] = m_state[1 + i];
	}

	m_state[ORDER - 1] = item;
}

class MarkovGraph
{
public:
	template<typename T>
	MarkovGraph(const T& iterable);

	bool generate();

private:
	using transition_t = std::unordered_map<item_t, int>;

	void add_state(const State& state, const item_t& i);
	void add_input(const input_t& input);

	void iteration(std::string& result) const;
	bool check(const std::string& name) const;

	std::unordered_map<State, transition_t, State::Hash> m_graph;
	std::unordered_set<std::string> used;
};

template<typename T>
MarkovGraph::MarkovGraph(const T& iterable)
{
	for (auto i = iterable.begin(); i != iterable.end(); ++i)
	{
		const input_t input = *i;
		add_input(input);
		used.insert(*i);
	}
}

bool MarkovGraph::generate()
{
	std::string result;
	bool passed_check = false;
	int attempt = 0;
	do
	{
		++attempt;
		result.clear();
		iteration(result);
		passed_check = check(result);
	} while (!passed_check && 100 > attempt);

	if (passed_check)
	{
		std::cout << result << std::endl;
		used.insert(result);
	}
	else
	{
		std::cout << "Solution has not been bound after " << attempt << " attempts." << std::endl;
	}

	return passed_check;
}

void MarkovGraph::add_state(const State& state, const item_t& i)
{
	const auto state_i = m_graph.find(state);
	if (m_graph.end() == state_i)
	{
		transition_t transition;
		transition[i] = 1;
		m_graph.emplace(state, transition);

		return;
	}

	transition_t& transition = state_i->second;
	auto transition_i = transition.find(i);
	if (transition.end() == transition_i)
	{
		transition[i] = 1;

		return;
	}

	++transition_i->second;
}

void MarkovGraph::add_input(const input_t& input)
{
	State state;
	for (std::size_t i = 0; input[i] != ZERO; ++i)
	{
		add_state(state, input[i]);

		state.push(input[i]);
	}

	add_state(state, ZERO);
}

void MarkovGraph::iteration(std::string& result) const
{
	State state;
	do
	{
		const auto i = m_graph.find(state);
		if (i == m_graph.end())
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

				state.push(s.first);
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

bool MarkovGraph::check(const std::string& name) const
{
	if (5 > name.length())
	{
		return false;
	}

	if (10 < name.length())
	{
		return false;
	}

	if (used.find(name) != used.end())
	{
		return false;
	}

	return true;
}

bool read_file(const char* file_name, std::list<std::string>&
	names)
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

int main(int argc, char** argv)
{
	if (0 == argc)
	{
		std::cerr << "Usage: " << argv[0] << " <name of the file with samples>" << std::endl;
		return 1;
	}

	std::list<std::string> names;
	if (!read_file(argv[1], names))
	{
		perror("couldn't read from file");
		return 1;
	}

	MarkovGraph graph(names);

	int counter = 0;
	std::srand(time(nullptr));
	while (graph.generate() && ++counter != 25);

	return 0;
}
