#ifndef GARGOYLE_NODEMESSAGE_H
#define GARGOYLE_NODEMESSAGE_H
/*
   These functions gives a nice BMessage interface to node attributes,
   by letting you transfer attributes to and from BMessages.  It makes
   it so you can use all the convenient Find...() and Add...() functions
   provided by BMessage for attributes too.  You use it as follows:

   BMessage m;
   BNode n(path);
   if (reading) { n>>m; printf("woohoo=%s\n",m.FindString("woohoo")) }
   else { m.AddString("woohoo","it's howdy doody time"); n<<m; }
   
   If there is more than one data item with a given name, the first
   item is the one writen to the node.
*/

#include <Node.h>
#include <Message.h>

#ifdef B_BEOS_VERSION_DANO
#define _IMPEXP_MAIL
#endif

_IMPEXP_MAIL BNode& operator<<(BNode& n, const BMessage& m);
_IMPEXP_MAIL BNode& operator>>(BNode& n, BMessage& m);
inline const BMessage& operator>>(const BMessage& m, BNode& n){n<<m;return m;}
inline       BMessage& operator<<(      BMessage& m, BNode& n){n>>m;return m;}

#endif
