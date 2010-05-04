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

		effect_compiler_impl::effect_compiler_impl(CComPtr<IDirect3DDevice9> dev)
			: dev(dev)
		{
		}

		bool effect_compiler_impl::compile_fragment(service_ptr_t<effect_handle>& effect, pfc::list_t<effect_compiler::diagnostic_entry>& output, pfc::string const& source)
		{
			effect = nullptr;
			output.remove_all();

			if (source.get_length() == 0)
				return false;
				
			std::vector<char> fx_body(source.get_ptr(), source.get_ptr() + source.get_length());
			if (size_t diff = nuke_if(fx_body, [](char c) { return (unsigned char)c >= 0x80U; }))
			{
				diagnostic_entry e = { { 0, 0 }, "error", "", "Effect contained non-ASCII code units. Remove any characters with diacritics or other moonspeak.\n" };
				output.add_item(e);
			}

			{
				CComPtr<ID3DXEffect> fx;
				CComPtr<ID3DXBuffer> err;
				HRESULT hr = S_OK;
				DWORD flags = 0;
#if defined(_DEBUG)
				flags |= D3DXSHADER_DEBUG | D3DXSHADER_OPTIMIZATION_LEVEL0;
#endif
				hr = D3DXCreateEffect(dev, &fx_body[0], fx_body.size(), nullptr, nullptr, flags, nullptr, &fx, &err);
				if (FAILED(hr))
				{
					pfc::list_t<diagnostic_entry> errors;
					typedef char const* iter;
					if (err)
					{
						iter first = (char*)err->GetBufferPointer(), last = first + err->GetBufferSize();
						qi::parse(first, last, error_grammar<iter>(), errors);
						output.add_items(errors);
					}
					return false;
				}
				effect = new service_impl_t<effect_impl>(fx);
			}
			return true;
		}

		effect_impl::effect_impl(CComPtr<ID3DXEffect> fx)
			: fx(fx)
		{}

		CComPtr<ID3DXEffect> effect_impl::get_effect() const
		{
			return fx;
		}

		
		pfc::string simple_diagnostic_format(pfc::list_t<effect_compiler::diagnostic_entry> const& in)
		{
			using karma::int_;
			using karma::string;

			typedef std::back_insert_iterator<std::string> Iter;
			karma::rule<Iter, effect_compiler::diagnostic_entry::location()> loc = '(' << int_ << ',' << int_ << "): ";

			std::vector<std::string> lines;
			in.enumerate([&lines, &loc](effect_compiler::diagnostic_entry const& e)
			{
				std::string out;
				auto sink = std::back_inserter(out);

				karma::generate(sink,
					loc << string << ": " << string << ": " << string , e);
				lines.push_back(out);
			});

			std::string out;
			auto sink = std::back_inserter(out);
			karma::generate(sink, string % "\n", lines);
			return pfc::string(out.c_str());
		}
	}
}