/**
 * \file Column.h
 * \author Petr Velan <petr.velan@cesnet.cz>
 * \brief Header of class for managing columns
 *
 * Copyright (C) 2011 CESNET, z.s.p.o.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of the Company nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * ALTERNATIVELY, provided that this notice is retained in full, this
 * product may be distributed under the terms of the GNU General Public
 * License (GPL) version 2 or later, in which case the provisions
 * of the GPL apply INSTEAD OF those given above.
 *
 * This software is provided ``as is, and any express or implied
 * warranties, including, but not limited to, the implied warranties of
 * merchantability and fitness for a particular purpose are disclaimed.
 * In no event shall the company or contributors be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute
 * goods or services; loss of use, data, or profits; or business
 * interruption) however caused and on any theory of liability, whether
 * in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even
 * if advised of the possibility of such damage.
 *
 */

#ifndef COLUMN_H_
#define COLUMN_H_

#include "typedefs.h"
#include "AST.h"
#include "Cursor.h"

namespace ipfixdump {

/* Cursor depends on Column class */
class Cursor;

/**
 * \brief Class with information about one column
 */
class Column
{
public:

	/**
	 * \brief Initialise column from xml configuration
	 *
	 * @param doc pugi xml document
	 * @param alias alias of the new column
	 * @param aggregate Aggregate mode
	 * @return true when initialization completed, false on error
	 */
	bool init(pugi::xml_document &doc, std::string alias, bool aggregate);

	/**
	 * \brief Returns column name (that should be printed in header)
	 *
	 * @return Column name
	 */
	std::string getName();

	/**
	 * \brief Sets name to "name"
	 * @param name New name for the column
	 */
	void setName(std::string name);

	/**
	 * \brief Returns set of column aliases
	 *
	 * @return Set of column aliases
	 */
	stringSet getAliases();

	/**
	 * \brief Add alias to current set of aliases
	 * @param alias Aliass to add
	 */
	void addAlias(std::string alias);

	/**
	 * \brief Returns width of the column (specified in XML)
	 * @return width of the column
	 */
	int getWidth();

	/**
	 * \brief Set column width to "width"
	 * @param width New width of the column
	 */
	void setWidth(int width);

	/**
	 * \brief Returns true when column should be aligned to the left
	 * @return true when column should be aligned to the left, false otherwise
	 */
	bool getAlignLeft();

	/**
	 * \brief Set column alignment
	 * @param alignLeft True when column should be aligned to the left
	 */
	void setAlignLeft(bool alignLeft);

	/**
	 * \brief Column class constructor
	 */
	Column();

	/**
	 * \brief Returns value of current column in row specified by cursor
	 *
	 * structure values contains required value in form of union
	 *
	 * @param cur cursor pointing to current row
	 * @return values structure containing required value
	 */
	values* getValue(Cursor *cur);

	/**
	 * \brief Can this column be used in aggregation?
	 * @return true when column is aggregable
	 */
	bool getAggregate();

	/**
	 * \brief Returns set of table column names that this column works with
	 *
	 * @return Set of table column names
	 */
	stringSet getColumns();

	/**
	 * \brief Set AST for this column
	 *
	 * @param ast AST for this column
	 */
	void setAST(AST *ast);

	/**
	 * \brief Sets columns aggregation mode
	 *
	 * This is important for function getColumns(), which should know
	 * whether to return column names with aggregation function i.e. sum(e0id1)
	 *
	 * @param aggregation
	 */
	void setAggregation(bool aggregation);

	/**
	 * \brief Returns string to print when value is not available
	 *
	 * @return string to print when value is not available
	 */
	std::string getNullStr();

	/**
	 * \brief Returns semantics of the column
	 * This is used to specify formatting of the column
	 *
	 * @return semantics of the column
	 */
	std::string getSemantics();

	/**
	 * \brief Returns true if column is a separator column
	 *
	 * @return returns true if column is a separator column, false otherwise
	 */
	bool isSeparator();

	/**
	 * \brief Returns true when column represents an operation
	 *
	 * @return true when column represents an operation
	 */
	bool isOperation();

	/**
	 * \brief Class destructor
	 * Frees AST
	 */
	~Column();

private:

	/**
	 * \brief Evaluates AST against data in cursor
	 *
	 * @param ast AST to evaluate
	 * @param cur Cursor with data
	 * @return returns values structure
	 */
	values *evaluate(AST *ast, Cursor *cur);

		/**
	 * \brief Performs operation on given data
	 *
	 * Can change value type (bigger integer)
	 *
	 * @param left left operand
	 * @param right right operand
	 * @param op one of '+', '-', '/', '*'
	 * @return return resulting value of appropriate type
	 */
	values* performOperation(values *left, values *right, unsigned char op);

	/**
	 * \brief Create element of type value from XMLnode element
	 *
	 * @param element XML node element
	 * @param doc XML document with configuration
	 * @return AST structure of created element
	 */
	AST* createValueElement(pugi::xml_node element, pugi::xml_document &doc);

	/**
	 * \brief Create element of type operation from XMLnode element
	 *
	 * @param operation XML node element
	 * @param doc XML document with configuration
	 * @return AST structure of created operation
	 */
	AST* createOperationElement(pugi::xml_node operation, pugi::xml_document &doc);

	/**
	 * \brief Return column names used in this AST
	 *
	 * @param ast to go through
	 * @return Set of column names
	 */
	stringSet& getColumns(AST* ast);

	/**
	 * \brief Is AST aggregable?
	 *
	 * @param ast AST to check
	 * @return true when AST is aggregable
	 */
	bool getAggregate(AST* ast);

	std::string nullStr; /**< String to print when columns has no value */
	std::string name; /**< name of the column  */
	int width; /**< Width of the column */
	bool alignLeft; /**< Align column to left?*/
	AST *ast; /**< Abstract syntax tree for value of this column */
	stringSet aliases; /**< Aliases of the column*/
	bool aggregation; /**< Determines whether column is in aggregation mode */

}; /* end of Column class */

} /* end of ipfixdump namespace */

#endif /* COLUMN_H_ */
