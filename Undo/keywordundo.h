#ifndef KEYWORDUNDO_H
#define KEYWORDUNDO_H

#include <QStringList>
#include "Cache/catalog.h"

/*
    One image's keywords as a catalog-wide rewrite changed them (Undo plan P4): the
    catalog row it was found by, the two verbatim lists it carried, and the two it was
    written with. MW::rewriteKeywords records one per file written while a rewrite is
    open (beginKeywordRewrite / endKeywordRewrite); Edit > Undo writes the old lists
    back through the same writer, Redo the new ones (MW::applyKeywordRewrite).
*/
struct KeywordRewrite
{
    CatalogRow row;
    QStringList oldSubject, oldHierarchical;
    QStringList newSubject, newHierarchical;
};

#endif // KEYWORDUNDO_H
