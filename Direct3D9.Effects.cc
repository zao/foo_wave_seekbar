#include "PchSeekbar.h"
#include "Direct3D.h"
#include "Direct3D.Effects.h"
#include "Helpers.h"
#include "resource.h"

#include "PfcSpirit.h"
#include <boost/spirit/include/qi.hpp>

namespace qi = boost::spirit::qi;

namespace ascii = boost::spirit::ascii;
namespace karma = boost::spirit::karma;
namespace qi = boost::spirit::qi;

namespace wave
{
	namespace direct3d9
	{
		typedef effect_compiler::diagnostic_entry entry;
		typedef pfc::list_t<entry> entry_list;

		template <typename Iterator>
		struct error_grammar : qi::grammar<Iterator, entry_list()>
		{
			error_grammar()
				: error_grammar::base_type(root)
			{
				using qi::int_;
				using qi::lexeme;
				using qi::lit;
				using qi::omit;
				using qi::repeat;
				using qi::eol;
				using ascii::alpha;
				using ascii::char_;
				using ascii::digit;
				using ascii::string;

				message %= +char_;
				code %= alpha >> repeat(4)[digit] >> ':';
				type %= +alpha;
				position %= '(' >> int_ >> ',' >> int_ >> "):";
				path %= +(char_ - position);
				line %= omit[path] >> position >> ' ' >> type >> ' ' >> code >> ' ' >> message;
				root %= +line;
			}

			qi::rule<Iterator, entry_list()> root;
			qi::rule<Iterator, entry()> line;
			qi::rule<Iterator, std::string()> path;
			qi::rule<Iterator, entry::location()> position;
			qi::rule<Iterator, std::string()> type, code, message;
		};

		effect_compiler_impl::effect_compiler_impl(weak_ptr<frontend_impl> fe, CComPtr<IDirect3DDevice9> dev)
			: fe(fe), dev(dev)
		{
			get_resource_contents(fx_header, IDR_DEFAULT_FX_HEADER);
			get_resource_contents(fx_footer, IDR_DEFAULT_FX_FOOTER);
			offset_lines = 1 + std::count(begin(fx_header), end(fx_header), '\n');
		}

		bool effect_compiler_impl::compile_fragment(service_ptr_t<effect_handle>& effect, pfc::list_t<effect_compiler::diagnostic_entry>& output, pfc::string const& source)
		{
			effect = nullptr;
			output.remove_all();

			auto front = fe.lock();
			if (!front)
				return false;
				
			std::vector<char> fx_body(source.get_ptr(), source.get_ptr() + source.get_length());
			if (size_t diff = nuke_if(fx_body, [](char c) { return (unsigned char)c >= 0x80U; }))
			{
				diagnostic_entry e = { { 0, 0 }, "error", "", "Effect contained non-ASCII code units. Remove any characters with diacritics or other moonspeak.\n" };
				output.add_item(e);
			}

			std::string fx_source;
			{
				using namespace karma;
				generate(std::back_inserter(fx_source), *char_ << "\n" << *char_ << "\n" << *char_ << "\n", fx_header, fx_body, fx_footer);
			}

			{
				CComPtr<ID3DXEffect> fx;
				CComPtr<ID3DXBuffer> err;
				HRESULT hr = S_OK;
				hr = D3DXCreateEffect(dev, &fx_source[0], fx_source.size(), nullptr, nullptr, 0, nullptr, &fx, &err);
				if (FAILED(hr))
				{
					pfc::list_t<diagnostic_entry> errors;
					typedef char const* iter;
					iter first = (char*)err->GetBufferPointer(), last = first + err->GetBufferSize();
					qi::parse(first, last, error_grammar<iter>(), errors);

					for (t_size idx = 0u, n = errors.get_count(); idx < n; ++idx)
					{
						errors[idx].loc.row -= offset_lines;
					}

					output.add_items(errors);
					return false;
				}
			}
			return true;
		}
	}
}