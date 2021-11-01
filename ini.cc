#include <iostream>
#include <fstream>
#include <optional>
#include <vector>

struct INI
{
	struct Node
	{
		enum class Kind
		{
			Section,
			Key,
			Value
		};

		Kind kind;
		std::string value;
	};

	std::vector<Node> nodes;

	template<typename T>
	void for_all_sections(T &&t)
	{
		for (auto const& node : nodes) {
			if (node.kind == Node::Kind::Section)
				t(node.value);
		}
	}

	template<typename T>
	bool for_section(std::string_view name, T &&t)
	{
		for (auto i = 0u; i < nodes.size(); ++i)
			if (auto const& node = nodes[i]; node.kind == Node::Kind::Section && node.value == name) {
				for (i += 1; i < nodes.size() - 1; i += 2) {
					if (nodes[i].kind != Node::Kind::Key || nodes[i+1].kind != Node::Kind::Value)
						break;

					t(nodes[i].value, nodes[i+1].value);
				}
				return true;
			}

		return false;
	}

	static std::optional<INI> from_file(std::string const& filename);
	static std::optional<INI> from_string(std::string const& content);
};

std::optional<INI> INI::from_file(std::string const& filename)
{
	std::ifstream file(filename);
	if (!file) {
		return std::nullopt;
	}

	std::string content { std::istreambuf_iterator<char>(file), {} };
	return from_string(content);
}

std::optional<INI> INI::from_string(std::string const& content)
{
	INI ini;

	std::string_view src = content;

	while (!src.empty()) {
		while (!src.empty() && std::isspace(src.front())) src.remove_prefix(1);

		if (src.empty())
			break;

		auto &new_node = ini.nodes.emplace_back();

		if (src.front() == '[') {
			new_node.kind = Node::Kind::Section;

			auto const end = src.find(']');
			if (end == std::string_view::npos)
				return std::nullopt;

			new_node.value.assign(src.begin() + 1, src.begin() + end);
			src.remove_prefix(end + 1);
		} else {
			new_node.kind = Node::Kind::Key;

			auto const key_end = src.find('=');
			if (key_end == std::string_view::npos)
				return std::nullopt;

			new_node.value.assign(src.begin(), src.begin() + key_end);

			auto &value_node = ini.nodes.emplace_back();
			value_node.kind = Node::Kind::Value;

			auto const value_end = src.find('\n');
			value_node.value.assign(src.begin() + key_end + 1, src.begin() + value_end);
			src.remove_prefix(value_end + 1);
		}
	}

	return { ini };
}
