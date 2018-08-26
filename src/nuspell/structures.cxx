/* Copyright 2018 Dimitrij Mijoski
 *
 * This file is part of Nuspell.
 *
 * Nuspell is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Nuspell is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with Nuspell.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file structures.cxx
 * Data structures.
 */

#include "structures.hxx"
#include "string_utils.hxx"

namespace nuspell {

using namespace std;

// template class String_Set<char>;
// template class String_Set<wchar_t>;
template class String_Set<char16_t>;

template <class CharT>
void Substr_Replacer<CharT>::sort_uniq()
{
	auto first = begin(table);
	auto last = end(table);
	sort(first, last, [](auto& a, auto& b) { return a.first < b.first; });
	auto it = unique(first, last,
	                 [](auto& a, auto& b) { return a.first == b.first; });
	table.erase(it, last);

	// remove empty key ""
	if (!table.empty() && table.front().first.empty())
		table.erase(begin(table));
}

namespace {
template <class CharT>
struct Comparer_Str_Rep {

	using StrT = basic_string<CharT>;
	using StrViewT = my_string_view<CharT>;

	auto static cmp_prefix_of(const StrT& p, StrViewT of)
	{
		return p.compare(0, p.npos, of.data(),
		                 min(p.size(), of.size()));
	}
	auto operator()(const pair<StrT, StrT>& a, StrViewT b)
	{
		return cmp_prefix_of(a.first, b) < 0;
	}
	auto operator()(StrViewT a, const pair<StrT, StrT>& b)
	{
		return cmp_prefix_of(b.first, a) > 0;
	}
	auto static eq(const pair<StrT, StrT>& a, StrViewT b)
	{
		return cmp_prefix_of(a.first, b) == 0;
	}
};

template <class CharT>
auto find_match(const typename Substr_Replacer<CharT>::Table_Pairs& t,
                my_string_view<CharT> s)
{
	Comparer_Str_Rep<CharT> csr;
	auto it = begin(t);
	auto last_match = end(t);
	for (;;) {
		auto it2 = upper_bound(it, end(t), s, csr);
		if (it2 == it) {
			// not found, s is smaller that the range
			break;
		}
		--it2;
		if (csr.eq(*it2, s)) {
			// Match found. Try another search maybe for
			// longer.
			last_match = it2;
			it = ++it2;
		}
		else {
			// not found, s is greater that the range
			break;
		}
	}
	return last_match;
}
} // namespace

template <class CharT>
auto Substr_Replacer<CharT>::replace(StrT& s) const -> StrT&
{

	if (table.empty())
		return s;
	for (size_t i = 0; i < s.size(); /*no increment here*/) {
		auto substr = my_string_view<CharT>(&s[i], s.size() - i);
		auto it = find_match(table, substr);
		if (it != end(table)) {
			auto& match = *it;
			// match found. match.first is the found string,
			// match.second is the replacement.
			s.replace(i, match.first.size(), match.second);
			i += match.second.size();
			continue;
		}
		++i;
	}
	return s;
}

template class Substr_Replacer<char>;
template class Substr_Replacer<wchar_t>;

template <class CharT>
auto Break_Table<CharT>::order_entries() -> void
{
	auto it = remove_if(begin(table), end(table), [](auto& s) {
		return s.empty() ||
		       (s.size() == 1 && (s[0] == '^' || s[0] == '$'));
	});
	table.erase(it, end(table));

	auto is_start_word_break = [=](auto& x) { return x[0] == '^'; };
	auto is_end_word_break = [=](auto& x) { return x.back() == '$'; };
	auto start_word_breaks_last =
	    partition(begin(table), end(table), is_start_word_break);
	start_word_breaks_last_idx = start_word_breaks_last - begin(table);

	for_each(begin(table), start_word_breaks_last,
	         [](auto& e) { e.erase(0, 1); });

	auto end_word_breaks_last =
	    partition(start_word_breaks_last, end(table), is_end_word_break);
	end_word_breaks_last_idx = end_word_breaks_last - begin(table);

	for_each(start_word_breaks_last, end_word_breaks_last,
	         [](auto& e) { e.pop_back(); });
}
template class Break_Table<char>;
template class Break_Table<wchar_t>;

template class Prefix<char>;
template class Prefix<wchar_t>;
template class Suffix<char>;
template class Suffix<wchar_t>;

auto Compound_Rule_Table::fill_all_flags() -> void
{
	for (auto& f : rules) {
		all_flags += f;
	}
	all_flags.erase(u'?');
	all_flags.erase(u'*');
}

struct Out_Iter_One_Bool {
	bool* value = nullptr;
	auto& operator++() { return *this; }
	auto& operator++(int) { return *this; }
	auto& operator*() { return *this; }
	auto& operator=(char16_t)
	{
		*value = true;
		return *this;
	}
};

auto Compound_Rule_Table::has_any_of_flags(const Flag_Set& f) const -> bool
{
	auto has_intersection = false;
	auto out_it = Out_Iter_One_Bool{&has_intersection};
	set_intersection(begin(all_flags), end(all_flags), begin(f), end(f),
	                 out_it);
	return has_intersection;
}

auto match_compund_rule(const std::vector<const Flag_Set*>& words_data,
                        const u16string& pattern)
{
	return match_simple_regex(
	    words_data, pattern,
	    [](const Flag_Set* d, char16_t p) { return d->contains(p); });
}

auto Compound_Rule_Table::match_any_rule(
    const std::vector<const Flag_Set*> data) const -> bool
{
	return any_of(begin(rules), end(rules), [&](const u16string& p) {
		return match_compund_rule(data, p);
	});
}

template <class CharT>
auto Replacement_Table<CharT>::order_entries() -> void
{
	auto it = remove_if(begin(table), end(table), [](auto& p) {
		auto& s = p.first;
		return s.empty() ||
		       (s.size() == 1 && (s[0] == '^' || s[0] == '$'));
	});
	table.erase(it, end(table));

	auto is_start_word_break = [=](auto& x) { return x.first[0] == '^'; };
	auto is_end_word_break = [=](auto& x) { return x.first.back() == '$'; };
	auto start_word_breaks_last =
	    partition(begin(table), end(table), is_start_word_break);
	start_word_reps_last_idx = start_word_breaks_last - begin(table);

	for_each(begin(table), start_word_breaks_last,
	         [](auto& e) { e.first.erase(0, 1); });

	auto end_word_breaks_last =
	    partition(start_word_breaks_last, end(table), is_end_word_break);
	end_word_reps_last_idx = end_word_breaks_last - begin(table);

	for_each(start_word_breaks_last, end_word_breaks_last,
	         [](auto& e) { e.first.pop_back(); });
}
template class Replacement_Table<char>;
template class Replacement_Table<wchar_t>;

} // namespace nuspell
