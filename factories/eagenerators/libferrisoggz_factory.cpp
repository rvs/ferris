/******************************************************************************
*******************************************************************************
*******************************************************************************

    libferris
    Copyright (C) 2001 Ben Martin

    libferris is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    libferris is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with libferris.  If not, see <http://www.gnu.org/licenses/>.

    For more details see the COPYING file in the root directory of this
    distribution.

    $Id: libferrisoggz_factory.cpp,v 1.2 2010/09/24 21:31:29 ben Exp $

*******************************************************************************
*******************************************************************************
******************************************************************************/

#include <FerrisEAPlugin.hh>
#include "config.h"

using namespace std;
namespace Ferris
{


static Factory::EndingList EL()
{
    static Factory::EndingList r;
    static bool virgin = true;
    
    if( virgin )
    {
        virgin = false;
        
#ifdef LIBOGGZ_FOR_OGG
        r.push_back( make_pair( string("name"), string(".oggz") ));
#endif
        r.push_back( make_pair( string("name"), string(".should-not-match___") ));
    }
    
    return r;
}

    /**
     *
     * New method that allows the factory itself to be statically bound
     * to libferris but the plugin to be dynamically loaded. (1.1.10)+
     *
     */
    namespace 
    {
        static bool r = RegisterEAGeneratorModule(
             Factory::ComposeEndsWithMatcher( EL() ),
             AUTOTOOLS_CONFIG_LIBDIR + "/ferris/plugins/eagenerators/libferrisoggz.so",
            "oggz",
            false,
             true, // has-state
             AttributeCreator::CREATE_PRI_NATURALP1 );
    };
};
