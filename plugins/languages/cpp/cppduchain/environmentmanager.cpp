/***************************************************************************
   Copyright 2006 David Nolden <david.nolden.kdevelop@art-master.de>
***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "environmentmanager.h"
#include <QFileInfo>
#include <kdebug.h>
#include "rpp/pp-macro.h"
#include "rpp/pp-environment.h"
#include <language/interfaces/iproblem.h>
#include "cpppreprocessenvironment.h"
#include <language/duchain/repositories/itemrepository.h>
#include <language/editor/modificationrevision.h>
#include "parser/rpp/macrorepository.h"
#include "cppdebughelper.h"
#include <language/duchain/arrayhelpers.h>
#include <language/duchain/duchainregister.h>

using namespace KDevelop;

DEFINE_LIST_MEMBER_HASH(IncludePathListItem, m_includePaths, KDevelop::IndexedString)

struct IncludePathListItem {
  
    IncludePathListItem() {
      initializeAppendedLists(true);
    }
    ~IncludePathListItem() {
      freeAppendedLists();
    }
    
    void operator=(const IncludePathListItem& rhs) {
      m_refCount = 0; //We can use this as a kind of initialization, since it's only called by createItem(..)
      copyListsFrom(rhs);
    }
    
    bool operator==(const IncludePathListItem& rhs) const {
      return listsEqual(rhs);
    }
  
    uint hash() const {
      uint ret = 0;
      for(int a = 0; a < m_includePathsSize(); ++a)
        ret = (m_includePaths()[a].hash() + ret) * 17;
      
      return ret;
    }
    
    uint itemSize() const {
      return dynamicSize();
    }
    
    uint classSize() const {
      return sizeof(*this);
    }
  
    uint m_refCount;
    START_APPENDED_LISTS(IncludePathListItem);
    APPENDED_LIST_FIRST(IncludePathListItem, KDevelop::IndexedString, m_includePaths);
    END_APPENDED_LISTS(IncludePathListItem, m_includePaths);
};

typedef AppendedListItemRequest<IncludePathListItem, 40*4> IncludePathsRequest;

typedef KDevelop::ItemRepository<IncludePathListItem, IncludePathsRequest, KDevelop::NoDynamicData, false> IncludePathsRepository;
IncludePathsRepository includePathsRepository("include path repository");

bool Cpp::EnvironmentManager::m_simplifiedMatching = false;
//  #define LEXERCACHE_DEBUG
//  #define ifDebug(X) X
//If DYNAMIC_DEBUGGING is defined, debugging can be started at any point in runtime,
//by calling setIsDebugging(true) from within the debugger
// #define DYNAMIC_DEBUGGING

#ifdef DYNAMIC_DEBUGGING
volatile bool is_debugging = false;

bool debugging() {
  return is_debugging;
}

//Ment to be called from within dbg, to start doing debug output at a specific point
void setIsDebugging(bool is) {
  is_debugging = is;
}

#define LEXERCACHE_DEBUG
#define ifDebug(x) if(debugging()) {x;}
#else
inline bool debugging() {
  return false;
}
#endif

using namespace Cpp;


REGISTER_DUCHAIN_ITEM(EnvironmentFile);

//Repository that contains the actual macros, and maps them to indices
MacroDataRepository Cpp::EnvironmentManager::macroDataRepository("macro repository");
//Set-repository that contains the string-sets
Utils::BasicSetRepository Cpp::EnvironmentManager::stringSetRepository("string sets", true);
//Set-repository that contains the macro-sets
Utils::BasicSetRepository Cpp::EnvironmentManager::macroSetRepository("macro sets", true);

const rpp::pp_macro& Cpp::MacroIndexConversion::toItem(uint index) const {
  return *EnvironmentManager::macroDataRepository.itemFromIndex( index );
}

uint Cpp::MacroIndexConversion::toIndex(const rpp::pp_macro& macro) const {
  return EnvironmentManager::macroDataRepository.index( MacroRepositoryItemRequest(macro) );
}

//For debugging
QString id(const EnvironmentFile* file) {
  return file->url().str() + QString(" %1").arg((size_t)file) ;
}

//Only for debugging
QString print(const Utils::Set& s) {
  QString ret;
  bool first = true;
  StringSetIterator it( s );
  while(it) {
    if(!first)
      ret += ", ";
    first = false;
    
    ret += (*it).str();
    ++it;
  }
  return ret;
}

QString print(const Cpp::LazyStringSet& set) {
  return print( set.set() );
}

QString print(const Cpp::LazyMacroSet& set) {
  QString ret;
  bool first = true;
  MacroSetIterator it( set.set() );
  while(it) {
    if(!first)
      ret += ", ";
    first = false;
    
    ret += it.ref().toString();
    ++it;
  }
  return ret;
}

void EnvironmentManager::setSimplifiedMatching(bool simplified) {
  m_simplifiedMatching = simplified;
}

bool EnvironmentManager::isSimplifiedMatching() {
  return m_simplifiedMatching;
}

bool EnvironmentFile::matchEnvironment(const ParsingEnvironment* _environment) const {
  const CppPreprocessEnvironment* cppEnvironment = dynamic_cast<const CppPreprocessEnvironment*>(_environment);
  if(!cppEnvironment)
    return false;
  
  Utils::Set environmentMacroNames = cppEnvironment->macroNameSet().set();
  
  if( cppEnvironment->identityOffsetRestrictionEnabled() && cppEnvironment->identityOffsetRestriction() != identityOffset() ) {
#ifdef LEXERCACHE_DEBUG
    kDebug( 9007 ) << "file" << url().str() << "does not match branching hash. Restriction:" << cppEnvironment->identityOffsetRestriction() << "Actual:" << identityOffset();
#endif
    return false;
  }
  
  Utils::Set conflicts = (environmentMacroNames & strings()) - m_usedMacroNames.set();

  for( StringSetIterator it( conflicts ); it; ++it ) {
    rpp::pp_macro* m = cppEnvironment->retrieveStoredMacro( *it );
    if(m && !m->isUndef()) {
      
#ifdef LEXERCACHE_DEBUG
      if(debugging()) {
        kDebug(9007) << "The environment contains a macro that can affect the cached file, but that should not exist:" << m->name.str();
      }
#endif
      return false;
    }
  }
  
  //Make sure that all external macros used by the file now exist too

  ///@todo find out why this assertion sometimes triggers, maybe different macros with the same name were used?
  //ifDebug( Q_ASSERT(m_usedMacros.set().count() == m_usedMacroNames.set().count()) );
  ifDebug( kDebug(9007) << "Count of used macros that need to be verified:" << m_usedMacros.set().count() );

  for ( MacroSetIterator it( m_usedMacros.set() ); it; ++it ) {
    rpp::pp_macro* m = cppEnvironment->retrieveStoredMacro( it.ref().name );
    if ( !m || !(*m == it.ref()) ) {
      if( !m && it.ref().isUndef() ) {
        ifDebug( kDebug( 9007 ) << "Undef-macro" << it.ref().name.str() << "is ok" << m );
        //It is okay, we did not find a macro, but the used macro is an undef macro
        //Q_ASSERT(0); //Undef-macros should not be marked as used
      } else {
        ifDebug( kDebug( 9007 ) << "The cached file " << url().str() << " used a macro called \"" << it.ref().name.str() << "\"(from" << it.ref().file.str() << "), but the environment" << (m ? "contains differing macro of that name" : "does not contain that macro") << ", the cached file is not used"  );
        ifDebug( if(m) { kDebug() << "Used macro: " << it.ref().toString()  << "from" << it.ref().file.str() << "found:" << m->toString() << "from" << m->file.str(); } );
        return false;
      }
    }else{
      ifDebug( kDebug( 9007 ) << it.ref().name.str() << "match" );
    }
  }

  ifDebug( kDebug( 9007 ) << "Using cached file " << url().str() );  
  return true;
}

bool EnvironmentFile::needsUpdate() const {
  return ParsingEnvironmentFile::needsUpdate();
}

EnvironmentFile::EnvironmentFile( IndexedString url, TopDUContext* topContext ) : ParsingEnvironmentFile(*new EnvironmentFileData(), url), /*m_includeFiles(&EnvironmentManager::stringSetRepository), */m_missingIncludeFiles(&EnvironmentManager::stringSetRepository), m_usedMacros(&EnvironmentManager::macroSetRepository), m_usedMacroNames(&EnvironmentManager::stringSetRepository), m_definedMacros(&EnvironmentManager::macroSetRepository), m_definedMacroNames(&EnvironmentManager::stringSetRepository), m_unDefinedMacroNames(&EnvironmentManager::stringSetRepository) {

  d_func_dynamic()->setClassId(this);
  
  d_func_dynamic()->m_topContext = IndexedTopDUContext(topContext);
  
  d_func_dynamic()->m_url = url;

//   ifDebug( kDebug(9007) << "created for" << url.str() << "modification-time:" << d_func_dynamic()->m_modificationTime );
  
  clearModificationRevisions();
}

EnvironmentFile::EnvironmentFile( EnvironmentFileData& data ) : ParsingEnvironmentFile(data) 
//       , m_includeFiles(&EnvironmentManager::stringSetRepository, 0, Utils::Set(data.m_includeFiles, &EnvironmentManager::stringSetRepository)) 
      , m_missingIncludeFiles(&EnvironmentManager::stringSetRepository, 0, Utils::Set(data.m_missingIncludeFiles, &EnvironmentManager::stringSetRepository))
      , m_usedMacros(&EnvironmentManager::macroSetRepository, 0, Utils::Set(data.m_usedMacros, &EnvironmentManager::macroSetRepository))
      , m_usedMacroNames(&EnvironmentManager::stringSetRepository, 0, Utils::Set(data.m_usedMacroNames, &EnvironmentManager::stringSetRepository))
      , m_definedMacros(&EnvironmentManager::macroSetRepository, 0, Utils::Set(data.m_definedMacros, &EnvironmentManager::macroSetRepository))
      , m_definedMacroNames(&EnvironmentManager::stringSetRepository, 0, Utils::Set(data.m_definedMacroNames, &EnvironmentManager::stringSetRepository))
      , m_unDefinedMacroNames(&EnvironmentManager::stringSetRepository, 0, Utils::Set(data.m_unDefinedMacroNames, &EnvironmentManager::stringSetRepository))
{
}

EnvironmentFile::~EnvironmentFile() {
}

void EnvironmentFile::aboutToSave() {
  ParsingEnvironmentFile::aboutToSave();
  if(d_func()->m_missingIncludeFiles != m_missingIncludeFiles.set().setIndex())
    d_func_dynamic()->m_missingIncludeFiles = m_missingIncludeFiles.set().setIndex();
  if(d_func()->m_usedMacros != m_usedMacros.set().setIndex())
    d_func_dynamic()->m_usedMacros = m_usedMacros.set().setIndex();
  if(d_func()->m_usedMacroNames != m_usedMacroNames.set().setIndex())
    d_func_dynamic()->m_usedMacroNames = m_usedMacroNames.set().setIndex();
  if(d_func()->m_definedMacros != m_definedMacros.set().setIndex())
    d_func_dynamic()->m_definedMacros = m_definedMacros.set().setIndex();
  if(d_func()->m_definedMacroNames != m_definedMacroNames.set().setIndex())
    d_func_dynamic()->m_definedMacroNames = m_definedMacroNames.set().setIndex();
  if(d_func()->m_unDefinedMacroNames != m_unDefinedMacroNames.set().setIndex())
    d_func_dynamic()->m_unDefinedMacroNames = m_unDefinedMacroNames.set().setIndex();
}

void EnvironmentFile::setContentStartLine(int line) {
  d_func_dynamic()->m_contentStartLine = line;
}

int EnvironmentFile::contentStartLine() const {
  return d_func()->m_contentStartLine;
}

void EnvironmentFile::addDefinedMacro( const rpp::pp_macro& macro, const rpp::pp_macro* previousOfSameName ) {
#ifdef LEXERCACHE_DEBUG
  if(debugging()) {
  kDebug( 9007 )  << id(this) << "defined macro" << macro.name.str();
  }
#endif
  if( previousOfSameName && m_definedMacros.contains(*previousOfSameName) ) ///@todo Make this faster. We cannot remove the definedMacros.contains(..), because else we get problems.
    m_definedMacros.remove( *previousOfSameName );
  else if( m_definedMacroNames.contains(macro.name) ) {
    //Search if there is already a macro of the same name in the set, and remove it
    //This is slow, but should not happen too often
    ///@todo maybe give a warning, and find out how this can happen
    for( MacroSetIterator it( m_definedMacros.set() ); it; ++it )
      if( macro.name == it.ref().name )
        m_definedMacros.remove(it.ref());
  }
  
  if(macro.isUndef()) {
    m_definedMacroNames.remove( macro.name );
    
    m_unDefinedMacroNames.insert( macro.name );
  } else {
    m_unDefinedMacroNames.remove( macro.name );
    m_definedMacroNames.insert( macro.name );
    
    m_definedMacros.insert( macro );
  }
  
  //State: If it is an undef macro, it is not in m_definedMacroNames not in m_definedMacros, and it is in m_unDefinedMacroNames
  //       If  it is a normal macro, it is in m_definedMacroNames, it is in m_definedMacros, and it is not in m_unDefinedMacroNames
}

void EnvironmentFile::usingMacro( const rpp::pp_macro& macro ) {
  if ( !m_definedMacroNames.contains( macro.name ) && !m_unDefinedMacroNames.contains( macro.name ) && !macro.isUndef() ) {
#ifdef LEXERCACHE_DEBUG
  if(debugging()) {
    kDebug( 9007 ) << id(this) << "used macro" << macro.name.str() << "from" << macro.file.str();
  }
#endif
    m_usedMacros.insert( macro );
  
    m_usedMacroNames.insert( macro.name );
  }
}

// const IndexedStringSet& EnvironmentFile::includeFiles() const {
//   return m_includeFiles;
// }

Utils::Set EnvironmentFile::strings() const {
  return Utils::Set(d_func()->m_strings, &EnvironmentManager::stringSetRepository);
}


///Set of all defined macros, including those of all deeper included files
const LazyMacroSet& EnvironmentFile::definedMacros() const {
  return m_definedMacros;
}

///Set of all macros used from outside, including those used in deeper included files
const LazyStringSet& EnvironmentFile::usedMacroNames() const {
  return m_usedMacroNames;
}

///Set of all macros used from outside, including those used in deeper included files
const LazyStringSet& EnvironmentFile::definedMacroNames() const {
  return m_definedMacroNames;
}

const LazyStringSet& EnvironmentFile::unDefinedMacroNames() const {
  return m_unDefinedMacroNames;
}

///Set of all macros used from outside, including those used in deeper included files
const LazyMacroSet& EnvironmentFile::usedMacros() const {
  return m_usedMacros;
}

const QList<IndexedString> EnvironmentFile::includePaths() const {
  QList<IndexedString> ret;
  if(d_func()->m_includePaths) {
    const IncludePathListItem* item = includePathsRepository.itemFromIndex(d_func()->m_includePaths);
    
    FOREACH_FUNCTION(IndexedString include, item->m_includePaths)
      ret << include;
  }
  return ret;
}

void EnvironmentFile::setIncludePaths( const QList<IndexedString>& paths ) {
  if(d_func()->m_includePaths) {
    IncludePathListItem* item = includePathsRepository.dynamicItemFromIndex(d_func()->m_includePaths);
    --item->m_refCount;
    if(!item->m_refCount)
      includePathsRepository.deleteItem(d_func()->m_includePaths);
    d_func_dynamic()->m_includePaths = 0;
  }
  if(!paths.isEmpty()) {
    IncludePathListItem item;
    foreach(const IndexedString &include, paths)
      item.m_includePathsList().append(include);
    d_func_dynamic()->m_includePaths = includePathsRepository.index(item);
    IncludePathListItem* gotItem = includePathsRepository.dynamicItemFromIndex(d_func()->m_includePaths);
    ++gotItem->m_refCount;
  }
}

void EnvironmentFile::addMissingIncludeFile(const IndexedString& file)
{
  m_missingIncludeFiles.insert(file);
}

const LazyStringSet& EnvironmentFile::missingIncludeFiles() const
{
  return m_missingIncludeFiles;
}

void EnvironmentFile::clearMissingIncludeFiles()
{
  m_missingIncludeFiles.clear();
}

void EnvironmentFile::addIncludeFile( const IndexedString& file, const ModificationRevision& modificationTime ) {
//   m_includeFiles.insert(file);

  addModificationRevision(file, modificationTime);
}

void EnvironmentFile::addStrings( const std::set<Utils::BasicSetRepository::Index>& strings ) {
  Utils::Set oldSet(d_func()->m_strings, &EnvironmentManager::stringSetRepository);
  Utils::Set newSet = (this->strings() + EnvironmentManager::stringSetRepository.createSet(strings));
  d_func_dynamic()->m_strings = newSet.setIndex();

/*  newSet.staticRef();
  oldSet.staticUnref();*/
}

//The parameter should be a EnvironmentFile that was lexed AFTER the content of this file
void EnvironmentFile::merge( const EnvironmentFile& file ) {
  ///@todo Do reference-counting, so no useless string-sets remain from the temporary operations done here
  
#ifdef LEXERCACHE_DEBUG
  if(debugging()) {
  kDebug( 9007 ) <<  id(this) << ": merging" << id(&file)  << "defined in macros this:" << print(m_definedMacroNames)  << "defined macros in other:" << print(file.m_definedMacroNames) << "undefined macros in other:" << print(file.m_unDefinedMacroNames) << "strings in other:" << print(file.strings());
  }
#endif
  {
    Utils::Set oldSet(d_func()->m_strings, &EnvironmentManager::stringSetRepository);
    Utils::Set newSet = (strings() + (file.strings() - m_definedMacroNames.set()) - m_unDefinedMacroNames.set());
    d_func_dynamic()->m_strings = newSet.setIndex();

/*    newSet.staticRef();
    oldSet.staticUnref();*/
  }
  ///@todo Probably it's more efficient having 2 sets m_changedMacroNames and m_unDefinedMacroNames, where m_unDefinedMacroNames is a subset of m_changedMacroNames.  
  //Only add macros to the usedMacros-set that were not defined locally
  m_usedMacroNames += (file.m_usedMacroNames.set() - m_definedMacroNames.set()) - m_unDefinedMacroNames.set();

  ///Merge those used macros that were not defined within this environment
  //This is slightly inefficient, would be nicer to have a fast mechanism for this
  
  {
    Utils::Set definedMacroNamesSet = m_definedMacroNames.set();
    Utils::Set unDefinedMacroNamesSet = m_unDefinedMacroNames.set();
    
    for(MacroSetIterator it( file.m_usedMacros.set() ); it; ++it) {
      const rpp::pp_macro& macro(it.ref());
      if( !definedMacroNamesSet.contains(macro.name.index()) && !unDefinedMacroNamesSet.contains(macro.name.index()) )
        m_usedMacros.insertIndex( it.index() );
    }
  }
  
  ifDebug( Q_ASSERT(m_usedMacroNames.set().count() == m_usedMacros.set().count()) );
  
  ///Add defined macros from the merged file.

  {
    Utils::Set otherDefinedMacroNamesSet = file.m_definedMacroNames.set();
    Utils::Set otherUnDefinedMacroNamesSet = file.m_unDefinedMacroNames.set();
    //Since merged macros overrule already stored ones, first remove the ones of the same name.
    for( MacroSetIterator it( m_definedMacros.set() ); it; ++it ) {
      const rpp::pp_macro& macro(it.ref());
      if( otherDefinedMacroNamesSet.contains( macro.name.index() ) || otherUnDefinedMacroNamesSet.contains( macro.name.index() ) )
        m_definedMacros.remove(macro);
    }
  }

  //Now merge in the new defined macros
  
  m_unDefinedMacroNames += file.m_unDefinedMacroNames.set();
  m_unDefinedMacroNames -= file.m_definedMacroNames.set();
  m_definedMacroNames -= file.m_unDefinedMacroNames.set();
  m_definedMacroNames += file.m_definedMacroNames.set();
  m_definedMacros += file.m_definedMacros.set();

  ///Merge include-files, problems and other stuff
//   m_includeFiles += file.m_includeFiles.set();
  m_missingIncludeFiles += file.m_missingIncludeFiles.set();
  
  addModificationRevisions(file.allModificationRevisions());

#ifdef LEXERCACHE_DEBUG
  if(debugging()) {
  kDebug( 9007 ) << id(this) << ": defined macro names in this after merge:" << m_definedMacroNames.set().count() << print(m_definedMacroNames);
  kDebug( 9007 ) << id(this) << ": defined in this after merge:" << m_definedMacros.set().count() << print(m_definedMacros);
  ifDebug( Q_ASSERT(m_definedMacros.set().count() == m_definedMacroNames.set().count()) );
  kDebug( 9007 ) << id(this) << ": undefined in this after merge:" << print(m_unDefinedMacroNames);
  kDebug( 9007 ) << id(this) << ": strings in this after merge:" << print(strings());
  kDebug( 9007 ) << id(this) << ": macros used in this after merge:" << print(m_usedMacroNames);
  }
#endif
}

size_t EnvironmentFile::hash() const {
  ///@todo remove the (size_t)(this), it is just temporary to make them unique, but will not work with serialization to disk.
  ///Instead, create a hash over the contained strings, and make sure the other hashes work reliably.
  return (size_t)(this); //m_usedMacros.valueHash() + m_usedMacros.idHash() + m_definedMacros.idHash() + m_definedMacros.valueHash() + (size_t)(this)/*+ m_strings.hash()*/; ///@todo is the string-hash needed here?
}

uint EnvironmentFile::identityOffset() const {
  return d_func()->m_identityOffset;
}

void EnvironmentFile::setIdentityOffset(uint offset) {
  d_func_dynamic()->m_identityOffset = offset;
}

int EnvironmentFile::type() const {
  return CppParsingEnvironment;
}