#pragma once
//------------------------------------------------------------------------------
/**
    @file   memdb/table.h

    Contains declarations for tables.

    @copyright
    (C) 2020 Individual contributors, see AUTHORS file
*/
//------------------------------------------------------------------------------
#include "ids/id.h"
#include "util/arrayallocator.h"
#include "util/fixedarray.h"
#include "util/string.h"
#include "util/stringatom.h"
#include "util/hashtable.h"
#include "componentid.h"
#include "tablesignature.h"
#include "util/bitfield.h"

namespace MemDb
{

/// Table identifier
ID_32_TYPE(TableId);

/// row identifier
typedef IndexT Row;

constexpr Row InvalidRow = -1;

/// column id
ID_16_TYPE(ColumnIndex);

/// information for creating a table
struct TableCreateInfo
{
    /// name to be given to the table
    Util::String name;
    /// array of components the table should initially have
    ComponentId const* components;
    /// number of columns
    SizeT numComponents;
};

//------------------------------------------------------------------------------
/**
    A table hold components as columns, and buffers for those columns.
    Property descriptions are retrieved from the MemDb::TypeRegistry

    @see    memdb/typeregistry.h
    @todo   This should be changed to SOAs
*/
struct Table
{
    using ColumnBuffer = void*;
    /// table identifier
    TableId tid = TableId::Invalid();
    /// name of the table
    Util::StringAtom name;
    /// number of rows
    uint32_t numRows = 0;
    /// total number of rows allocated
    uint32_t capacity = 128;
    /// initial grow. Gets doubled when table is saturated and expanded
    uint32_t grow = 128;
    // holds freed indices/rows to be reused in the table.
    Util::Array<IndexT> freeIds;
    /// all components that this table has
    Util::Array<ComponentId> components;
    /// can be used to keep track if the columns has been reallocated
    uint32_t version = 0;
	
	// TODO: partition the tables into chunks
	//struct Partition
	//{
	//	static constexpr uint	 capacity = 4096;  // total capacity (in elements) that the partition can contain
	//	uint64_t				 version = 0;	   // bump the version if you change anything about the partition
	//	void* buffer = nullptr;					   // contains the data
	//	Util::BitField<capacity> modified;		   // check a bit if the value in the buffer has been modified, and you need to track it
	//	uint32_t			     size = 0;		   // current number of elements in the buffer;
	//};
	
	/// holds all the column buffers. This excludes non-typed components
	Util::ArrayAllocator<ComponentId, ColumnBuffer> columns;
	/// maps componentid -> index in columns array
    Util::HashTable<ComponentId, IndexT, 32, 1> columnRegistry;
    /// allocation heap used for the column buffers
    static constexpr Memory::HeapType HEAP_MEMORY_TYPE = Memory::HeapType::DefaultHeap;
};

} // namespace MemDb
