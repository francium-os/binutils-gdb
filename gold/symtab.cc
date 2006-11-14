// symtab.cc -- the gold symbol table

#include "gold.h"

#include <cassert>
#include <stdint.h>
#include <string>
#include <utility>

#include "object.h"
#include "dynobj.h"
#include "output.h"
#include "target.h"
#include "workqueue.h"
#include "symtab.h"

namespace gold
{

// Class Symbol.

// Initialize fields in Symbol.  This initializes everything except u_
// and source_.

void
Symbol::init_fields(const char* name, const char* version,
		    elfcpp::STT type, elfcpp::STB binding,
		    elfcpp::STV visibility, unsigned char nonvis)
{
  this->name_ = name;
  this->version_ = version;
  this->got_offset_ = 0;
  this->type_ = type;
  this->binding_ = binding;
  this->visibility_ = visibility;
  this->nonvis_ = nonvis;
  this->is_target_special_ = false;
  this->is_def_ = false;
  this->is_forwarder_ = false;
  this->in_dyn_ = false;
  this->has_got_offset_ = false;
  this->has_warning_ = false;
}

// Initialize the fields in the base class Symbol for SYM in OBJECT.

template<int size, bool big_endian>
void
Symbol::init_base(const char* name, const char* version, Object* object,
		  const elfcpp::Sym<size, big_endian>& sym)
{
  this->init_fields(name, version, sym.get_st_type(), sym.get_st_bind(),
		    sym.get_st_visibility(), sym.get_st_nonvis());
  this->u_.from_object.object = object;
  // FIXME: Handle SHN_XINDEX.
  this->u_.from_object.shnum = sym.get_st_shndx();
  this->source_ = FROM_OBJECT;
  this->in_dyn_ = object->is_dynamic();
}

// Initialize the fields in the base class Symbol for a symbol defined
// in an Output_data.

void
Symbol::init_base(const char* name, Output_data* od, elfcpp::STT type,
		  elfcpp::STB binding, elfcpp::STV visibility,
		  unsigned char nonvis, bool offset_is_from_end)
{
  this->init_fields(name, NULL, type, binding, visibility, nonvis);
  this->u_.in_output_data.output_data = od;
  this->u_.in_output_data.offset_is_from_end = offset_is_from_end;
  this->source_ = IN_OUTPUT_DATA;
}

// Initialize the fields in the base class Symbol for a symbol defined
// in an Output_segment.

void
Symbol::init_base(const char* name, Output_segment* os, elfcpp::STT type,
		  elfcpp::STB binding, elfcpp::STV visibility,
		  unsigned char nonvis, Segment_offset_base offset_base)
{
  this->init_fields(name, NULL, type, binding, visibility, nonvis);
  this->u_.in_output_segment.output_segment = os;
  this->u_.in_output_segment.offset_base = offset_base;
  this->source_ = IN_OUTPUT_SEGMENT;
}

// Initialize the fields in the base class Symbol for a symbol defined
// as a constant.

void
Symbol::init_base(const char* name, elfcpp::STT type,
		  elfcpp::STB binding, elfcpp::STV visibility,
		  unsigned char nonvis)
{
  this->init_fields(name, NULL, type, binding, visibility, nonvis);
  this->source_ = CONSTANT;
}

// Initialize the fields in Sized_symbol for SYM in OBJECT.

template<int size>
template<bool big_endian>
void
Sized_symbol<size>::init(const char* name, const char* version, Object* object,
			 const elfcpp::Sym<size, big_endian>& sym)
{
  this->init_base(name, version, object, sym);
  this->value_ = sym.get_st_value();
  this->symsize_ = sym.get_st_size();
}

// Initialize the fields in Sized_symbol for a symbol defined in an
// Output_data.

template<int size>
void
Sized_symbol<size>::init(const char* name, Output_data* od,
			 Value_type value, Size_type symsize,
			 elfcpp::STT type, elfcpp::STB binding,
			 elfcpp::STV visibility, unsigned char nonvis,
			 bool offset_is_from_end)
{
  this->init_base(name, od, type, binding, visibility, nonvis,
		  offset_is_from_end);
  this->value_ = value;
  this->symsize_ = symsize;
}

// Initialize the fields in Sized_symbol for a symbol defined in an
// Output_segment.

template<int size>
void
Sized_symbol<size>::init(const char* name, Output_segment* os,
			 Value_type value, Size_type symsize,
			 elfcpp::STT type, elfcpp::STB binding,
			 elfcpp::STV visibility, unsigned char nonvis,
			 Segment_offset_base offset_base)
{
  this->init_base(name, os, type, binding, visibility, nonvis, offset_base);
  this->value_ = value;
  this->symsize_ = symsize;
}

// Initialize the fields in Sized_symbol for a symbol defined as a
// constant.

template<int size>
void
Sized_symbol<size>::init(const char* name, Value_type value, Size_type symsize,
			 elfcpp::STT type, elfcpp::STB binding,
			 elfcpp::STV visibility, unsigned char nonvis)
{
  this->init_base(name, type, binding, visibility, nonvis);
  this->value_ = value;
  this->symsize_ = symsize;
}

// Class Symbol_table.

Symbol_table::Symbol_table()
  : size_(0), saw_undefined_(0), offset_(0), table_(), namepool_(),
    forwarders_(), commons_(), warnings_()
{
}

Symbol_table::~Symbol_table()
{
}

// The hash function.  The key is always canonicalized, so we use a
// simple combination of the pointers.

size_t
Symbol_table::Symbol_table_hash::operator()(const Symbol_table_key& key) const
{
  return key.first ^ key.second;
}

// The symbol table key equality function.  This is only called with
// canonicalized name and version strings, so we can use pointer
// comparison.

bool
Symbol_table::Symbol_table_eq::operator()(const Symbol_table_key& k1,
					  const Symbol_table_key& k2) const
{
  return k1.first == k2.first && k1.second == k2.second;
}

// Make TO a symbol which forwards to FROM.  

void
Symbol_table::make_forwarder(Symbol* from, Symbol* to)
{
  assert(from != to);
  assert(!from->is_forwarder() && !to->is_forwarder());
  this->forwarders_[from] = to;
  from->set_forwarder();
}

// Resolve the forwards from FROM, returning the real symbol.

Symbol*
Symbol_table::resolve_forwards(Symbol* from) const
{
  assert(from->is_forwarder());
  Unordered_map<Symbol*, Symbol*>::const_iterator p =
    this->forwarders_.find(from);
  assert(p != this->forwarders_.end());
  return p->second;
}

// Look up a symbol by name.

Symbol*
Symbol_table::lookup(const char* name, const char* version) const
{
  Stringpool::Key name_key;
  name = this->namepool_.find(name, &name_key);
  if (name == NULL)
    return NULL;

  Stringpool::Key version_key = 0;
  if (version != NULL)
    {
      version = this->namepool_.find(version, &version_key);
      if (version == NULL)
	return NULL;
    }

  Symbol_table_key key(name_key, version_key);
  Symbol_table::Symbol_table_type::const_iterator p = this->table_.find(key);
  if (p == this->table_.end())
    return NULL;
  return p->second;
}

// Resolve a Symbol with another Symbol.  This is only used in the
// unusual case where there are references to both an unversioned
// symbol and a symbol with a version, and we then discover that that
// version is the default version.  Because this is unusual, we do
// this the slow way, by converting back to an ELF symbol.

template<int size, bool big_endian>
void
Symbol_table::resolve(Sized_symbol<size>* to, const Sized_symbol<size>* from
                      ACCEPT_SIZE_ENDIAN)
{
  unsigned char buf[elfcpp::Elf_sizes<size>::sym_size];
  elfcpp::Sym_write<size, big_endian> esym(buf);
  // We don't bother to set the st_name field.
  esym.put_st_value(from->value());
  esym.put_st_size(from->symsize());
  esym.put_st_info(from->binding(), from->type());
  esym.put_st_other(from->visibility(), from->nonvis());
  esym.put_st_shndx(from->shnum());
  Symbol_table::resolve(to, esym.sym(), from->object());
}

// Add one symbol from OBJECT to the symbol table.  NAME is symbol
// name and VERSION is the version; both are canonicalized.  DEF is
// whether this is the default version.

// If DEF is true, then this is the definition of a default version of
// a symbol.  That means that any lookup of NAME/NULL and any lookup
// of NAME/VERSION should always return the same symbol.  This is
// obvious for references, but in particular we want to do this for
// definitions: overriding NAME/NULL should also override
// NAME/VERSION.  If we don't do that, it would be very hard to
// override functions in a shared library which uses versioning.

// We implement this by simply making both entries in the hash table
// point to the same Symbol structure.  That is easy enough if this is
// the first time we see NAME/NULL or NAME/VERSION, but it is possible
// that we have seen both already, in which case they will both have
// independent entries in the symbol table.  We can't simply change
// the symbol table entry, because we have pointers to the entries
// attached to the object files.  So we mark the entry attached to the
// object file as a forwarder, and record it in the forwarders_ map.
// Note that entries in the hash table will never be marked as
// forwarders.

template<int size, bool big_endian>
Symbol*
Symbol_table::add_from_object(Object* object,
			      const char *name,
			      Stringpool::Key name_key,
			      const char *version,
			      Stringpool::Key version_key,
			      bool def,
			      const elfcpp::Sym<size, big_endian>& sym)
{
  Symbol* const snull = NULL;
  std::pair<typename Symbol_table_type::iterator, bool> ins =
    this->table_.insert(std::make_pair(std::make_pair(name_key, version_key),
				       snull));

  std::pair<typename Symbol_table_type::iterator, bool> insdef =
    std::make_pair(this->table_.end(), false);
  if (def)
    {
      const Stringpool::Key vnull_key = 0;
      insdef = this->table_.insert(std::make_pair(std::make_pair(name_key,
								 vnull_key),
						  snull));
    }

  // ins.first: an iterator, which is a pointer to a pair.
  // ins.first->first: the key (a pair of name and version).
  // ins.first->second: the value (Symbol*).
  // ins.second: true if new entry was inserted, false if not.

  Sized_symbol<size>* ret;
  bool was_undefined;
  bool was_common;
  if (!ins.second)
    {
      // We already have an entry for NAME/VERSION.
      ret = this->get_sized_symbol SELECT_SIZE_NAME(size) (ins.first->second
                                                           SELECT_SIZE(size));
      assert(ret != NULL);

      was_undefined = ret->is_undefined();
      was_common = ret->is_common();

      Symbol_table::resolve(ret, sym, object);

      if (def)
	{
	  if (insdef.second)
	    {
	      // This is the first time we have seen NAME/NULL.  Make
	      // NAME/NULL point to NAME/VERSION.
	      insdef.first->second = ret;
	    }
	  else if (insdef.first->second != ret)
	    {
	      // This is the unfortunate case where we already have
	      // entries for both NAME/VERSION and NAME/NULL.
	      const Sized_symbol<size>* sym2;
	      sym2 = this->get_sized_symbol SELECT_SIZE_NAME(size) (
		insdef.first->second
                SELECT_SIZE(size));
	      Symbol_table::resolve SELECT_SIZE_ENDIAN_NAME(size, big_endian) (
		ret, sym2 SELECT_SIZE_ENDIAN(size, big_endian));
	      this->make_forwarder(insdef.first->second, ret);
	      insdef.first->second = ret;
	    }
	}
    }
  else
    {
      // This is the first time we have seen NAME/VERSION.
      assert(ins.first->second == NULL);

      was_undefined = false;
      was_common = false;

      if (def && !insdef.second)
	{
	  // We already have an entry for NAME/NULL.  Make
	  // NAME/VERSION point to it.
	  ret = this->get_sized_symbol SELECT_SIZE_NAME(size) (
              insdef.first->second
              SELECT_SIZE(size));
	  Symbol_table::resolve(ret, sym, object);
	  ins.first->second = ret;
	}
      else
	{
	  Sized_target<size, big_endian>* target =
	    object->sized_target SELECT_SIZE_ENDIAN_NAME(size, big_endian) (
		SELECT_SIZE_ENDIAN_ONLY(size, big_endian));
	  if (!target->has_make_symbol())
	    ret = new Sized_symbol<size>();
	  else
	    {
	      ret = target->make_symbol();
	      if (ret == NULL)
		{
		  // This means that we don't want a symbol table
		  // entry after all.
		  if (!def)
		    this->table_.erase(ins.first);
		  else
		    {
		      this->table_.erase(insdef.first);
		      // Inserting insdef invalidated ins.
		      this->table_.erase(std::make_pair(name_key,
							version_key));
		    }
		  return NULL;
		}
	    }

	  ret->init(name, version, object, sym);

	  ins.first->second = ret;
	  if (def)
	    {
	      // This is the first time we have seen NAME/NULL.  Point
	      // it at the new entry for NAME/VERSION.
	      assert(insdef.second);
	      insdef.first->second = ret;
	    }
	}
    }

  // Record every time we see a new undefined symbol, to speed up
  // archive groups.
  if (!was_undefined && ret->is_undefined())
    ++this->saw_undefined_;

  // Keep track of common symbols, to speed up common symbol
  // allocation.
  if (!was_common && ret->is_common())
    this->commons_.push_back(ret);

  return ret;
}

// Add all the symbols in a relocatable object to the hash table.

template<int size, bool big_endian>
void
Symbol_table::add_from_relobj(
    Sized_relobj<size, big_endian>* relobj,
    const unsigned char* syms,
    size_t count,
    const char* sym_names,
    size_t sym_name_size,
    Symbol** sympointers)
{
  // We take the size from the first object we see.
  if (this->get_size() == 0)
    this->set_size(size);

  if (size != this->get_size() || size != relobj->target()->get_size())
    {
      fprintf(stderr, _("%s: %s: mixing 32-bit and 64-bit ELF objects\n"),
	      program_name, relobj->name().c_str());
      gold_exit(false);
    }

  const int sym_size = elfcpp::Elf_sizes<size>::sym_size;

  const unsigned char* p = syms;
  for (size_t i = 0; i < count; ++i, p += sym_size)
    {
      elfcpp::Sym<size, big_endian> sym(p);
      elfcpp::Sym<size, big_endian>* psym = &sym;

      unsigned int st_name = psym->get_st_name();
      if (st_name >= sym_name_size)
	{
	  fprintf(stderr,
		  _("%s: %s: bad global symbol name offset %u at %lu\n"),
		  program_name, relobj->name().c_str(), st_name,
		  static_cast<unsigned long>(i));
	  gold_exit(false);
	}

      const char* name = sym_names + st_name;

      // A symbol defined in a section which we are not including must
      // be treated as an undefined symbol.
      unsigned char symbuf[sym_size];
      elfcpp::Sym<size, big_endian> sym2(symbuf);
      unsigned int st_shndx = psym->get_st_shndx();
      if (st_shndx != elfcpp::SHN_UNDEF
	  && st_shndx < elfcpp::SHN_LORESERVE
	  && !relobj->is_section_included(st_shndx))
	{
	  memcpy(symbuf, p, sym_size);
	  elfcpp::Sym_write<size, big_endian> sw(symbuf);
	  sw.put_st_shndx(elfcpp::SHN_UNDEF);
	  psym = &sym2;
	}

      // In an object file, an '@' in the name separates the symbol
      // name from the version name.  If there are two '@' characters,
      // this is the default version.
      const char* ver = strchr(name, '@');

      Symbol* res;
      if (ver == NULL)
	{
	  Stringpool::Key name_key;
	  name = this->namepool_.add(name, &name_key);
	  res = this->add_from_object(relobj, name, name_key, NULL, 0,
				      false, *psym);
	}
      else
	{
	  Stringpool::Key name_key;
	  name = this->namepool_.add(name, ver - name, &name_key);

	  bool def = false;
	  ++ver;
	  if (*ver == '@')
	    {
	      def = true;
	      ++ver;
	    }

	  Stringpool::Key ver_key;
	  ver = this->namepool_.add(ver, &ver_key);

	  res = this->add_from_object(relobj, name, name_key, ver, ver_key,
				      def, *psym);
	}

      *sympointers++ = res;
    }
}

// Add all the symbols in a dynamic object to the hash table.

template<int size, bool big_endian>
void
Symbol_table::add_from_dynobj(
    Sized_dynobj<size, big_endian>* dynobj,
    const unsigned char* syms,
    size_t count,
    const char* sym_names,
    size_t sym_name_size,
    const unsigned char* versym,
    size_t versym_size,
    const std::vector<const char*>* version_map)
{
  // We take the size from the first object we see.
  if (this->get_size() == 0)
    this->set_size(size);

  if (size != this->get_size() || size != dynobj->target()->get_size())
    {
      fprintf(stderr, _("%s: %s: mixing 32-bit and 64-bit ELF objects\n"),
	      program_name, dynobj->name().c_str());
      gold_exit(false);
    }

  if (versym != NULL && versym_size / 2 < count)
    {
      fprintf(stderr, _("%s: %s: too few symbol versions\n"),
	      program_name, dynobj->name().c_str());
      gold_exit(false);
    }

  const int sym_size = elfcpp::Elf_sizes<size>::sym_size;

  const unsigned char* p = syms;
  const unsigned char* vs = versym;
  for (size_t i = 0; i < count; ++i, p += sym_size, vs += 2)
    {
      elfcpp::Sym<size, big_endian> sym(p);

      // Ignore symbols with local binding.
      if (sym.get_st_bind() == elfcpp::STB_LOCAL)
	continue;

      unsigned int st_name = sym.get_st_name();
      if (st_name >= sym_name_size)
	{
	  fprintf(stderr, _("%s: %s: bad symbol name offset %u at %lu\n"),
		  program_name, dynobj->name().c_str(), st_name,
		  static_cast<unsigned long>(i));
	  gold_exit(false);
	}

      const char* name = sym_names + st_name;

      if (versym == NULL)
	{
	  Stringpool::Key name_key;
	  name = this->namepool_.add(name, &name_key);
	  this->add_from_object(dynobj, name, name_key, NULL, 0,
				false, sym);
	  continue;
	}

      // Read the version information.

      unsigned int v = elfcpp::Swap<16, big_endian>::readval(vs);

      bool hidden = (v & elfcpp::VERSYM_HIDDEN) != 0;
      v &= elfcpp::VERSYM_VERSION;

      if (v == static_cast<unsigned int>(elfcpp::VER_NDX_LOCAL))
	{
	  // This symbol should not be visible outside the object.
	  continue;
	}

      // At this point we are definitely going to add this symbol.
      Stringpool::Key name_key;
      name = this->namepool_.add(name, &name_key);

      if (v == static_cast<unsigned int>(elfcpp::VER_NDX_GLOBAL))
	{
	  // This symbol does not have a version.
	  this->add_from_object(dynobj, name, name_key, NULL, 0, false, sym);
	  continue;
	}

      if (v >= version_map->size())
	{
	  fprintf(stderr,
		  _("%s: %s: versym for symbol %zu out of range: %u\n"),
		  program_name, dynobj->name().c_str(), i, v);
	  gold_exit(false);
	}

      const char* version = (*version_map)[v];
      if (version == NULL)
	{
	  fprintf(stderr, _("%s: %s: versym for symbol %zu has no name: %u\n"),
		  program_name, dynobj->name().c_str(), i, v);
	  gold_exit(false);
	}

      Stringpool::Key version_key;
      version = this->namepool_.add(version, &version_key);

      // If this is an absolute symbol, and the version name and
      // symbol name are the same, then this is the version definition
      // symbol.  These symbols exist to support using -u to pull in
      // particular versions.  We do not want to record a version for
      // them.
      if (sym.get_st_shndx() == elfcpp::SHN_ABS && name_key == version_key)
	{
	  this->add_from_object(dynobj, name, name_key, NULL, 0, false, sym);
	  continue;
	}

      const bool def = !hidden && sym.get_st_shndx() != elfcpp::SHN_UNDEF;

      this->add_from_object(dynobj, name, name_key, version, version_key,
			    def, sym);
    }
}

// Create and return a specially defined symbol.  If ONLY_IF_REF is
// true, then only create the symbol if there is a reference to it.

template<int size, bool big_endian>
Sized_symbol<size>*
Symbol_table::define_special_symbol(Target* target, const char* name,
				    bool only_if_ref
                                    ACCEPT_SIZE_ENDIAN)
{
  assert(this->size_ == size);

  Symbol* oldsym;
  Sized_symbol<size>* sym;

  if (only_if_ref)
    {
      oldsym = this->lookup(name, NULL);
      if (oldsym == NULL || !oldsym->is_undefined())
	return NULL;
      sym = NULL;

      // Canonicalize NAME.
      name = oldsym->name();
    }
  else
    {
      // Canonicalize NAME.
      Stringpool::Key name_key;
      name = this->namepool_.add(name, &name_key);

      Symbol* const snull = NULL;
      const Stringpool::Key ver_key = 0;
      std::pair<typename Symbol_table_type::iterator, bool> ins =
	this->table_.insert(std::make_pair(std::make_pair(name_key, ver_key),
					   snull));

      if (!ins.second)
	{
	  // We already have a symbol table entry for NAME.
	  oldsym = ins.first->second;
	  assert(oldsym != NULL);
	  sym = NULL;
	}
      else
	{
	  // We haven't seen this symbol before.
	  assert(ins.first->second == NULL);

	  if (!target->has_make_symbol())
	    sym = new Sized_symbol<size>();
	  else
	    {
	      assert(target->get_size() == size);
	      assert(target->is_big_endian() ? big_endian : !big_endian);
	      typedef Sized_target<size, big_endian> My_target;
	      My_target* sized_target = static_cast<My_target*>(target);
	      sym = sized_target->make_symbol();
	      if (sym == NULL)
		return NULL;
	    }

	  ins.first->second = sym;
	  oldsym = NULL;
	}
    }

  if (oldsym != NULL)
    {
      assert(sym == NULL);

      sym = this->get_sized_symbol SELECT_SIZE_NAME(size) (oldsym
                                                           SELECT_SIZE(size));
      assert(sym->source() == Symbol::FROM_OBJECT);
      const int old_shnum = sym->shnum();
      if (old_shnum != elfcpp::SHN_UNDEF
	  && old_shnum != elfcpp::SHN_COMMON
	  && !sym->object()->is_dynamic())
	{
	  fprintf(stderr, "%s: linker defined: multiple definition of %s\n",
		  program_name, name);
	  // FIXME: Report old location.  Record that we have seen an
	  // error.
	  return NULL;
	}

      // Our new definition is going to override the old reference.
    }

  return sym;
}

// Define a symbol based on an Output_data.

void
Symbol_table::define_in_output_data(Target* target, const char* name,
				    Output_data* od,
				    uint64_t value, uint64_t symsize,
				    elfcpp::STT type, elfcpp::STB binding,
				    elfcpp::STV visibility,
				    unsigned char nonvis,
				    bool offset_is_from_end,
				    bool only_if_ref)
{
  assert(target->get_size() == this->size_);
  if (this->size_ == 32)
    this->do_define_in_output_data<32>(target, name, od, value, symsize,
				       type, binding, visibility, nonvis,
				       offset_is_from_end, only_if_ref);
  else if (this->size_ == 64)
    this->do_define_in_output_data<64>(target, name, od, value, symsize,
				       type, binding, visibility, nonvis,
				       offset_is_from_end, only_if_ref);
  else
    abort();
}

// Define a symbol in an Output_data, sized version.

template<int size>
void
Symbol_table::do_define_in_output_data(
    Target* target,
    const char* name,
    Output_data* od,
    typename elfcpp::Elf_types<size>::Elf_Addr value,
    typename elfcpp::Elf_types<size>::Elf_WXword symsize,
    elfcpp::STT type,
    elfcpp::STB binding,
    elfcpp::STV visibility,
    unsigned char nonvis,
    bool offset_is_from_end,
    bool only_if_ref)
{
  Sized_symbol<size>* sym;

  if (target->is_big_endian())
    sym = this->define_special_symbol SELECT_SIZE_ENDIAN_NAME(size, true) (
        target, name, only_if_ref
        SELECT_SIZE_ENDIAN(size, true));
  else
    sym = this->define_special_symbol SELECT_SIZE_ENDIAN_NAME(size, false) (
        target, name, only_if_ref
        SELECT_SIZE_ENDIAN(size, false));

  if (sym == NULL)
    return;

  sym->init(name, od, value, symsize, type, binding, visibility, nonvis,
	    offset_is_from_end);
}

// Define a symbol based on an Output_segment.

void
Symbol_table::define_in_output_segment(Target* target, const char* name,
				       Output_segment* os,
				       uint64_t value, uint64_t symsize,
				       elfcpp::STT type, elfcpp::STB binding,
				       elfcpp::STV visibility,
				       unsigned char nonvis,
				       Symbol::Segment_offset_base offset_base,
				       bool only_if_ref)
{
  assert(target->get_size() == this->size_);
  if (this->size_ == 32)
    this->do_define_in_output_segment<32>(target, name, os, value, symsize,
					  type, binding, visibility, nonvis,
					  offset_base, only_if_ref);
  else if (this->size_ == 64)
    this->do_define_in_output_segment<64>(target, name, os, value, symsize,
					  type, binding, visibility, nonvis,
					  offset_base, only_if_ref);
  else
    abort();
}

// Define a symbol in an Output_segment, sized version.

template<int size>
void
Symbol_table::do_define_in_output_segment(
    Target* target,
    const char* name,
    Output_segment* os,
    typename elfcpp::Elf_types<size>::Elf_Addr value,
    typename elfcpp::Elf_types<size>::Elf_WXword symsize,
    elfcpp::STT type,
    elfcpp::STB binding,
    elfcpp::STV visibility,
    unsigned char nonvis,
    Symbol::Segment_offset_base offset_base,
    bool only_if_ref)
{
  Sized_symbol<size>* sym;

  if (target->is_big_endian())
    sym = this->define_special_symbol SELECT_SIZE_ENDIAN_NAME(size, true) (
        target, name, only_if_ref
        SELECT_SIZE_ENDIAN(size, true));
  else
    sym = this->define_special_symbol SELECT_SIZE_ENDIAN_NAME(size, false) (
        target, name, only_if_ref
        SELECT_SIZE_ENDIAN(size, false));

  if (sym == NULL)
    return;

  sym->init(name, os, value, symsize, type, binding, visibility, nonvis,
	    offset_base);
}

// Define a special symbol with a constant value.  It is a multiple
// definition error if this symbol is already defined.

void
Symbol_table::define_as_constant(Target* target, const char* name,
				 uint64_t value, uint64_t symsize,
				 elfcpp::STT type, elfcpp::STB binding,
				 elfcpp::STV visibility, unsigned char nonvis,
				 bool only_if_ref)
{
  assert(target->get_size() == this->size_);
  if (this->size_ == 32)
    this->do_define_as_constant<32>(target, name, value, symsize,
				    type, binding, visibility, nonvis,
				    only_if_ref);
  else if (this->size_ == 64)
    this->do_define_as_constant<64>(target, name, value, symsize,
				    type, binding, visibility, nonvis,
				    only_if_ref);
  else
    abort();
}

// Define a symbol as a constant, sized version.

template<int size>
void
Symbol_table::do_define_as_constant(
    Target* target,
    const char* name,
    typename elfcpp::Elf_types<size>::Elf_Addr value,
    typename elfcpp::Elf_types<size>::Elf_WXword symsize,
    elfcpp::STT type,
    elfcpp::STB binding,
    elfcpp::STV visibility,
    unsigned char nonvis,
    bool only_if_ref)
{
  Sized_symbol<size>* sym;

  if (target->is_big_endian())
    sym = this->define_special_symbol SELECT_SIZE_ENDIAN_NAME(size, true) (
        target, name, only_if_ref
        SELECT_SIZE_ENDIAN(size, true));
  else
    sym = this->define_special_symbol SELECT_SIZE_ENDIAN_NAME(size, false) (
        target, name, only_if_ref
        SELECT_SIZE_ENDIAN(size, false));

  if (sym == NULL)
    return;

  sym->init(name, value, symsize, type, binding, visibility, nonvis);
}

// Define a set of symbols in output sections.

void
Symbol_table::define_symbols(const Layout* layout, Target* target, int count,
			     const Define_symbol_in_section* p)
{
  for (int i = 0; i < count; ++i, ++p)
    {
      Output_section* os = layout->find_output_section(p->output_section);
      if (os != NULL)
	this->define_in_output_data(target, p->name, os, p->value, p->size,
				    p->type, p->binding, p->visibility,
				    p->nonvis, p->offset_is_from_end,
				    p->only_if_ref);
      else
	this->define_as_constant(target, p->name, 0, p->size, p->type,
				 p->binding, p->visibility, p->nonvis,
				 p->only_if_ref);
    }
}

// Define a set of symbols in output segments.

void
Symbol_table::define_symbols(const Layout* layout, Target* target, int count,
			     const Define_symbol_in_segment* p)
{
  for (int i = 0; i < count; ++i, ++p)
    {
      Output_segment* os = layout->find_output_segment(p->segment_type,
						       p->segment_flags_set,
						       p->segment_flags_clear);
      if (os != NULL)
	this->define_in_output_segment(target, p->name, os, p->value, p->size,
				       p->type, p->binding, p->visibility,
				       p->nonvis, p->offset_base,
				       p->only_if_ref);
      else
	this->define_as_constant(target, p->name, 0, p->size, p->type,
				 p->binding, p->visibility, p->nonvis,
				 p->only_if_ref);
    }
}

// Set the final values for all the symbols.  Record the file offset
// OFF.  Add their names to POOL.  Return the new file offset.

off_t
Symbol_table::finalize(off_t off, Stringpool* pool)
{
  off_t ret;

  if (this->size_ == 32)
    ret = this->sized_finalize<32>(off, pool);
  else if (this->size_ == 64)
    ret = this->sized_finalize<64>(off, pool);
  else
    abort();

  // Now that we have the final symbol table, we can reliably note
  // which symbols should get warnings.
  this->warnings_.note_warnings(this);

  return ret;
}

// Set the final value for all the symbols.  This is called after
// Layout::finalize, so all the output sections have their final
// address.

template<int size>
off_t
Symbol_table::sized_finalize(off_t off, Stringpool* pool)
{
  off = align_address(off, size >> 3);
  this->offset_ = off;

  const int sym_size = elfcpp::Elf_sizes<size>::sym_size;
  Symbol_table_type::iterator p = this->table_.begin();
  size_t count = 0;
  while (p != this->table_.end())
    {
      Sized_symbol<size>* sym = static_cast<Sized_symbol<size>*>(p->second);

      // FIXME: Here we need to decide which symbols should go into
      // the output file.

      typename Sized_symbol<size>::Value_type value;

      switch (sym->source())
	{
	case Symbol::FROM_OBJECT:
	  {
	    unsigned int shnum = sym->shnum();

	    // FIXME: We need some target specific support here.
	    if (shnum >= elfcpp::SHN_LORESERVE
		&& shnum != elfcpp::SHN_ABS)
	      {
		fprintf(stderr, _("%s: %s: unsupported symbol section 0x%x\n"),
			program_name, sym->name(), shnum);
		gold_exit(false);
	      }

	    Object* symobj = sym->object();
	    if (symobj->is_dynamic())
	      {
		value = 0;
		shnum = elfcpp::SHN_UNDEF;
	      }
	    else if (shnum == elfcpp::SHN_UNDEF)
	      value = 0;
	    else if (shnum == elfcpp::SHN_ABS)
	      value = sym->value();
	    else
	      {
		Relobj* relobj = static_cast<Relobj*>(symobj);
		off_t secoff;
		Output_section* os = relobj->output_section(shnum, &secoff);

		if (os == NULL)
		  {
		    // We should be able to erase this symbol from the
		    // symbol table, but at least with gcc 4.0.2
		    // std::unordered_map::erase doesn't appear to return
		    // the new iterator.
		    // p = this->table_.erase(p);
		    ++p;
		    continue;
		  }

		value = sym->value() + os->address() + secoff;
	      }
	  }
	  break;

	case Symbol::IN_OUTPUT_DATA:
	  {
	    Output_data* od = sym->output_data();
	    value = sym->value() + od->address();
	    if (sym->offset_is_from_end())
	      value += od->data_size();
	  }
	  break;

	case Symbol::IN_OUTPUT_SEGMENT:
	  {
	    Output_segment* os = sym->output_segment();
	    value = sym->value() + os->vaddr();
	    switch (sym->offset_base())
	      {
	      case Symbol::SEGMENT_START:
		break;
	      case Symbol::SEGMENT_END:
		value += os->memsz();
		break;
	      case Symbol::SEGMENT_BSS:
		value += os->filesz();
		break;
	      default:
		abort();
	      }
	  }
	  break;

	case Symbol::CONSTANT:
	  value = sym->value();
	  break;

	default:
	  abort();
	}

      sym->set_value(value);
      pool->add(sym->name(), NULL);
      ++count;
      off += sym_size;
      ++p;
    }

  this->output_count_ = count;

  return off;
}

// Write out the global symbols.

void
Symbol_table::write_globals(const Target* target, const Stringpool* sympool,
			    Output_file* of) const
{
  if (this->size_ == 32)
    {
      if (target->is_big_endian())
	this->sized_write_globals<32, true>(target, sympool, of);
      else
	this->sized_write_globals<32, false>(target, sympool, of);
    }
  else if (this->size_ == 64)
    {
      if (target->is_big_endian())
	this->sized_write_globals<64, true>(target, sympool, of);
      else
	this->sized_write_globals<64, false>(target, sympool, of);
    }
  else
    abort();
}

// Write out the global symbols.

template<int size, bool big_endian>
void
Symbol_table::sized_write_globals(const Target*,
				  const Stringpool* sympool,
				  Output_file* of) const
{
  const int sym_size = elfcpp::Elf_sizes<size>::sym_size;
  unsigned char* psyms = of->get_output_view(this->offset_,
					     this->output_count_ * sym_size);
  unsigned char* ps = psyms;
  for (Symbol_table_type::const_iterator p = this->table_.begin();
       p != this->table_.end();
       ++p)
    {
      Sized_symbol<size>* sym = static_cast<Sized_symbol<size>*>(p->second);

      unsigned int shndx;
      switch (sym->source())
	{
	case Symbol::FROM_OBJECT:
	  {
	    unsigned int shnum = sym->shnum();

	    // FIXME: We need some target specific support here.
	    if (shnum >= elfcpp::SHN_LORESERVE
		&& shnum != elfcpp::SHN_ABS)
	      {
		fprintf(stderr, _("%s: %s: unsupported symbol section 0x%x\n"),
			program_name, sym->name(), sym->shnum());
		gold_exit(false);
	      }

	    Object* symobj = sym->object();
	    if (symobj->is_dynamic())
	      {
		// FIXME.
		shndx = elfcpp::SHN_UNDEF;
	      }
	    else if (shnum == elfcpp::SHN_UNDEF || shnum == elfcpp::SHN_ABS)
	      shndx = shnum;
	    else
	      {
		Relobj* relobj = static_cast<Relobj*>(symobj);
		off_t secoff;
		Output_section* os = relobj->output_section(shnum, &secoff);
		if (os == NULL)
		  continue;

		shndx = os->out_shndx();
	      }
	  }
	  break;

	case Symbol::IN_OUTPUT_DATA:
	  shndx = sym->output_data()->out_shndx();
	  break;

	case Symbol::IN_OUTPUT_SEGMENT:
	  shndx = elfcpp::SHN_ABS;
	  break;

	case Symbol::CONSTANT:
	  shndx = elfcpp::SHN_ABS;
	  break;

	default:
	  abort();
	}

      elfcpp::Sym_write<size, big_endian> osym(ps);
      osym.put_st_name(sympool->get_offset(sym->name()));
      osym.put_st_value(sym->value());
      osym.put_st_size(sym->symsize());
      osym.put_st_info(elfcpp::elf_st_info(sym->binding(), sym->type()));
      osym.put_st_other(elfcpp::elf_st_other(sym->visibility(),
					     sym->nonvis()));
      osym.put_st_shndx(shndx);

      ps += sym_size;
    }

  of->write_output_view(this->offset_, this->output_count_ * sym_size, psyms);
}

// Warnings functions.

// Add a new warning.

void
Warnings::add_warning(Symbol_table* symtab, const char* name, Object* obj,
		      unsigned int shndx)
{
  name = symtab->canonicalize_name(name);
  this->warnings_[name].set(obj, shndx);
}

// Look through the warnings and mark the symbols for which we should
// warn.  This is called during Layout::finalize when we know the
// sources for all the symbols.

void
Warnings::note_warnings(Symbol_table* symtab)
{
  for (Warning_table::iterator p = this->warnings_.begin();
       p != this->warnings_.end();
       ++p)
    {
      Symbol* sym = symtab->lookup(p->first, NULL);
      if (sym != NULL
	  && sym->source() == Symbol::FROM_OBJECT
	  && sym->object() == p->second.object)
	{
	  sym->set_has_warning();

	  // Read the section contents to get the warning text.  It
	  // would be nicer if we only did this if we have to actually
	  // issue a warning.  Unfortunately, warnings are issued as
	  // we relocate sections.  That means that we can not lock
	  // the object then, as we might try to issue the same
	  // warning multiple times simultaneously.
	  {
	    Task_locker_obj<Object> tl(*p->second.object);
	    const unsigned char* c;
	    off_t len;
	    c = p->second.object->section_contents(p->second.shndx, &len);
	    p->second.set_text(reinterpret_cast<const char*>(c), len);
	  }
	}
    }
}

// Issue a warning.  This is called when we see a relocation against a
// symbol for which has a warning.

void
Warnings::issue_warning(Symbol* sym, const std::string& location) const
{
  assert(sym->has_warning());
  Warning_table::const_iterator p = this->warnings_.find(sym->name());
  assert(p != this->warnings_.end());
  fprintf(stderr, _("%s: %s: warning: %s\n"), program_name, location.c_str(),
	  p->second.text.c_str());
}

// Instantiate the templates we need.  We could use the configure
// script to restrict this to only the ones needed for implemented
// targets.

template
void
Symbol_table::add_from_relobj<32, true>(
    Sized_relobj<32, true>* relobj,
    const unsigned char* syms,
    size_t count,
    const char* sym_names,
    size_t sym_name_size,
    Symbol** sympointers);

template
void
Symbol_table::add_from_relobj<32, false>(
    Sized_relobj<32, false>* relobj,
    const unsigned char* syms,
    size_t count,
    const char* sym_names,
    size_t sym_name_size,
    Symbol** sympointers);

template
void
Symbol_table::add_from_relobj<64, true>(
    Sized_relobj<64, true>* relobj,
    const unsigned char* syms,
    size_t count,
    const char* sym_names,
    size_t sym_name_size,
    Symbol** sympointers);

template
void
Symbol_table::add_from_relobj<64, false>(
    Sized_relobj<64, false>* relobj,
    const unsigned char* syms,
    size_t count,
    const char* sym_names,
    size_t sym_name_size,
    Symbol** sympointers);

template
void
Symbol_table::add_from_dynobj<32, true>(
    Sized_dynobj<32, true>* dynobj,
    const unsigned char* syms,
    size_t count,
    const char* sym_names,
    size_t sym_name_size,
    const unsigned char* versym,
    size_t versym_size,
    const std::vector<const char*>* version_map);

template
void
Symbol_table::add_from_dynobj<32, false>(
    Sized_dynobj<32, false>* dynobj,
    const unsigned char* syms,
    size_t count,
    const char* sym_names,
    size_t sym_name_size,
    const unsigned char* versym,
    size_t versym_size,
    const std::vector<const char*>* version_map);

template
void
Symbol_table::add_from_dynobj<64, true>(
    Sized_dynobj<64, true>* dynobj,
    const unsigned char* syms,
    size_t count,
    const char* sym_names,
    size_t sym_name_size,
    const unsigned char* versym,
    size_t versym_size,
    const std::vector<const char*>* version_map);

template
void
Symbol_table::add_from_dynobj<64, false>(
    Sized_dynobj<64, false>* dynobj,
    const unsigned char* syms,
    size_t count,
    const char* sym_names,
    size_t sym_name_size,
    const unsigned char* versym,
    size_t versym_size,
    const std::vector<const char*>* version_map);

} // End namespace gold.
