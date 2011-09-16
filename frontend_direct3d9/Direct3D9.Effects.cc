//          Copyright Lars Viklund 2008 - 2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "PchDirect3D9.h"
#include "Direct3D9.h"
#include "Direct3D9.Effects.h"
#include "resource.h"

#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/karma.hpp>

namespace qi = boost::spirit::qi;

namespace ascii = boost::spirit::ascii;
namespace karma = boost::spirit::karma;
namespace qi = boost::spirit::qi;

template <typename Cont, typename Pred>
typename Cont::size_type nuke_if(Cont& c, Pred p)
{
  auto count = c.size();
  c.erase(std::remove_if(begin(c), end(c), p), end(c));
  return count - c.size();
}

namespace wave
{
  namespace direct3d9
  {
    typedef effect_compiler::diagnostic_entry entry;
    typedef std::deque<entry> entry_list;

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

    bool effect_compiler_impl::compile_fragment(ref_ptr<effect_handle>& effect, array_sink<effect_compiler::diagnostic_entry> const& output, char const* source, size_t source_cb)
    {
			std::vector<effect_compiler::diagnostic_entry> errors;
      effect.reset();

      if (source_cb == 0)
        return false;
        
      std::vector<char> fx_body(source, source + source_cb);
      if (size_t diff = nuke_if(fx_body, [](char c) { return (unsigned char)c >= 0x80U; }))
      {
        diagnostic_entry e;
        diagnostic_entry::location loc = { 0, 0 };
        e.loc = loc;
        e.type = "error";
        e.code = "";
        e.message = "Effect contained non-ASCII code units. Remove any characters with diacritics or other moonspeak.\n";
        errors.push_back(e);
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
          std::deque<diagnostic_entry> errors;
          typedef char const* iter;
          if (err)
          {
            iter first = (char*)err->GetBufferPointer(), last = first + err->GetBufferSize();
            qi::parse(first, last, error_grammar<iter>(), errors);
            errors.insert(errors.end(), errors.begin(), errors.end());
          }
					output.set(errors.size() ? &errors[0] : 0, errors.size());
          return false;
        }
        effect.reset(new effect_impl(fx));
      }
			output.set(errors.size() ? &errors[0] : 0, errors.size());
      return true;
    }

    effect_impl::effect_impl(CComPtr<ID3DXEffect> fx)
      : fx(fx)
    {}

    CComPtr<ID3DXEffect> effect_impl::get_effect() const
    {
      return fx;
    }

    
    std::string simple_diagnostic_format(std::deque<effect_compiler::diagnostic_entry> const& in)
    {
      using karma::int_;
      using karma::string;

      typedef std::back_insert_iterator<std::string> Iter;
      karma::rule<Iter, effect_compiler::diagnostic_entry::location()> loc = '(' << int_ << ',' << int_ << "): ";

      std::vector<std::string> lines;
      std::for_each(in.begin(), in.end(),
        [&lines, &loc](effect_compiler::diagnostic_entry const& e)
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
      return out;
    }
  }
}
