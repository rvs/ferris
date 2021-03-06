/******************************************************************************
*******************************************************************************
*******************************************************************************

    libferris
    Copyright (C) 2011 Ben Martin

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

    $Id: libferrispostgresql.cpp,v 1.11 2009/04/18 21:30:09 ben Exp $

*******************************************************************************
*******************************************************************************
******************************************************************************/
#include <config.h>

#include <FerrisContextPlugin.hh>
#include <Trimming.hh>
#include <FerrisDOM.hh>
#include <libferrispastebinshared.hh>

#include "FerrisWebServices_private.hh"

#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/serialization/list.hpp>
#include <boost/serialization/set.hpp>
#include <boost/serialization/map.hpp>
#include <boost/serialization/version.hpp>

#include <QNetworkCookieJar>

using namespace std;
#define DEBUG cerr
//#define DEBUG LG_PASTEBIN_D


namespace Ferris
{
    using XML::domnode_list_t;
    
    extern "C"
    {
        FERRISEXP_EXPORT fh_context Brew( RootContextFactory* rf )
            throw( RootContextCreationFailed );
    };

    static DOMElement* getQPP( fh_domdoc doc )
    {
        DOMElement* root = doc->getDocumentElement();
        if( DOMElement* q = XML::getChildElement( root, "query" ) )
        {
            if( DOMElement* pages = XML::getChildElement( q, "pages" ) )
            {
                if( DOMElement* p = XML::getChildElement( pages, "page" ) )
                {
                    return p;
                }
            }
        }
        return 0;
    }
    static string getUID( const string& data )
    {
        fh_domdoc doc = Factory::StringToDOM( data );
        DOMElement* root = doc->getDocumentElement();
        if( DOMElement* q = XML::getChildElement( root, "query" ) )
        {
            if( DOMElement* uinfo = XML::getChildElement( q, "userinfo" ) )
            {
                std::string ret = getAttribute( uinfo, "id" );
                return ret;
            }
        }
        return "";
    }
    

    QNetworkAccessManager* getWikiQManager();
    
    
    
    /*******************************************************************************/
    /*******************************************************************************/
    /*******************************************************************************/
    /*******************************************************************************/
    /*******************************************************************************/

    std::string trimright( const std::string& ret, const std::string& ending )
    {
        if( !ends_with( ret, ending ))
            return ret;

        std::string x = ret.substr( 0, ret.length() - ending.length() );
        return x;
    }
    
    
    Wiki::Wiki( const std::string& serverURL )
        : m_serverURL( serverURL )
    {
        m_serverURL = trimright( m_serverURL, "index.php" );
    }
    Wiki::~Wiki()
    {
    }

    QUrl
    Wiki::getAPIUrl()
    {
        std::string earl = "http:/" + m_serverURL + "/api.php";
        std::string s = m_serverURL;
        if( contains( s, "kde.org" ))
            earl = "http://paste.kde.org/?";

        
        
        return QUrl( earl.c_str() );
    }

    std::string
    Wiki::request( stringmap_t args )
    {
        QByteArray postba;
        return request( args, postba );
    }

    
    std::string
    Wiki::request( stringmap_t args, const QByteArray& postba )
    {
        return request( tostr(getAPIUrl()), args, postba );
    }

    std::string
    Wiki::request( const std::string& earl, stringmap_t args, const QByteArray& postba )
    {
        if( args.end() == args.find("format"))
        {
            args["format"] = "xml";
        }
        
        QUrl u( earl.c_str() );
        addQueryItems( u, args );
        QNetworkRequest req(u);
        QNetworkAccessManager* qm = getWikiQManager();
        QNetworkReply* reply = qm->post( req, postba );
        connect( reply, SIGNAL( finished() ), SLOT( handleFinished() ) );
        m_waiter.block(reply);
            
        QByteArray ba = reply->readAll();
        DEBUG << "request() got response:" << tostr(ba) << endl;
        return tostr(ba);
    }
    
    void
    Wiki::ensureEditToken( const std::string& titles )
    {
        stringmap_t params;
        params["action"] = "query";
        params["prop"]   = "info|revisions";
        params["intoken"]= "edit";
        params["titles"] = titles;
        params["titles"] = "Main Page";
        string data = request( params );
        DEBUG << "ensureEditToken data:" << data << endl;

        fh_domdoc doc = Factory::StringToDOM( data );
        DOMElement* root = doc->getDocumentElement();
        if( DOMElement* q = XML::getChildElement( root, "query" ) )
        {
            if( DOMElement* pages = XML::getChildElement( q, "pages" ) )
            {
                if( DOMElement* p = XML::getChildElement( pages, "page" ) )
                {
                    std::string et = getAttribute( p, "edittoken" );
                    DEBUG << "edit token:" << et << endl;
                    m_editToken = et;
                }
            }
        }
    }
    
    void
    Wiki::ensureUploadToken()
    {
        if( m_uploadToken.empty() )
        {
            if( m_editToken.empty() )
                ensureEditToken();
            m_uploadToken = m_editToken;
            DEBUG << "m_uploadToken:" << m_uploadToken << endl;
        }
    }

    void
    Wiki::ensureMoveToken()
    {
        if( m_moveToken.empty() )
        {
            // if( m_editToken.empty() )
            //     ensureEditToken();
            // m_moveToken = m_editToken;
            // m_moveToken = Util::replace_all( m_moveToken, "+", "%2B" );
            
            stringmap_t params;
            params["action"] = "query";
            params["prop"]   = "info";
            params["intoken"]= "move";
            params["titles"] = "Main Page";
            string data = request( params );
            DEBUG << "ensureMoveToken data:" << data << endl;

            fh_domdoc doc = Factory::StringToDOM( data );
            DOMElement* root = doc->getDocumentElement();
            if( DOMElement* q = XML::getChildElement( root, "query" ) )
            {
                if( DOMElement* pages = XML::getChildElement( q, "pages" ) )
                {
                    if( DOMElement* p = XML::getChildElement( pages, "page" ) )
                    {
                        std::string t = getAttribute( p, "movetoken" );
                        DEBUG << "move token:" << t << endl;
                        m_moveToken = t;
//                        m_moveToken = Util::replace_all( m_moveToken, "+", "%2B" );
                    }
                }
            }
            
            DEBUG << "m_moveToken:" << m_moveToken << endl;
        }
    }
    
    

    void
    Wiki::streamingUploadComplete()
    {
        QNetworkReply* reply = m_reply;
        m_streamToQIO->writingComplete();
        QByteArray ba = m_streamToQIO->readResponse();
        string res = tostr(ba);
        DEBUG << "streamingUploadComplete() reply->error:" << reply->error() << endl;
        DEBUG << "result:" << res << endl;

        std::string earl = "http://paste.kde.org/";
        fh_domdoc doc = Factory::StringToDOM( res );
        DOMElement* root = doc->getDocumentElement();
        if( DOMElement* e = XML::getChildElement( root, "id" ) )
            earl += XML::getChildText(e) + "/";
        if( DOMElement* e = XML::getChildElement( root, "hash" ) )
            earl += XML::getChildText(e) + "/";
        cerr << earl << endl;
        
    }
    
    fh_iostream
    Wiki::getEditIOStream( int ContentLength, const std::string& project,
                           const std::string& password, bool isPrivate )
    {
//        ensureEditToken( project );

        stringmap_t params;
        params["paste_lang"]     = "text";
        params["paste_data"]     = "test123";
        params["api_submit"]     = "true";
        params["mode"]           = "xml";
        if( !project.empty() )
            params["paste_project"] = project;
        // params["bot"] = "1";
        // params["paste_user"]      = "xml";
        if( !password.empty() )
            params["paste_password"]  = password;
        if( isPrivate )
            params["paste_private"]   = "true";
        // params["paste_expire"]    = "xml";

        DEBUG << "cookies getEditIOStream() server url:" << tostr(getAPIUrl()) << endl;
        QNetworkAccessManager* qm = getWikiQManager();
        QUrl u( getAPIUrl() );
        addQueryItems( u, params );
        QNetworkRequest req(u);
        if( ContentLength )
            req.setHeader(QNetworkRequest::ContentLengthHeader, tostr(ContentLength).c_str() );
        req.setHeader(QNetworkRequest::ContentTypeHeader, "multipart/form-data" );

        std::string contentType = "text/plain";
        m_streamToQIO = Factory::createStreamToQIODevice();
        stringstream blobss;
        blobss << "Content-Disposition: form-data; name=\"paste_data\"\n"
               << "Content-Type: " << contentType;
        QNetworkReply* reply = m_streamToQIO->post( qm, req, blobss.str() );
        m_reply = reply;
        fh_iostream oss = m_streamToQIO->getStream();
        return oss;
    }
    
    fh_iostream
    Wiki::getUploadIOStream( int ContentLength, const std::string& title )
    {
        ensureUploadToken();
        
        stringmap_t params;
        params["action"]     = "upload";
        params["filename"]   = title;
//        params["comment"]    = "comment";
//        params["text"]       = "text";
        params["token"]      = m_uploadToken;
        params["format"]     = "xml";
        params["ignorewarnings"] = "1";
        
    
//        QNetworkAccessManager* qm = getQNonCachingManager();
        QNetworkAccessManager* qm = getWikiQManager();
        QUrl u( getAPIUrl() );
        addQueryItems( u, params );
        QNetworkRequest req(u);
        if( ContentLength )
            req.setHeader(QNetworkRequest::ContentLengthHeader, tostr(ContentLength).c_str() );
        req.setHeader(QNetworkRequest::ContentTypeHeader, "multipart/form-data" );

        std::string contentType = filenameToContextType( title );
        m_streamToQIO = Factory::createStreamToQIODevice();
        stringstream blobss;
        blobss << "Content-Disposition: form-data; name=\"file\"; filename=\"" << title << "\"" << "\n"
               << "Content-Type: " << contentType;
        QNetworkReply* reply = m_streamToQIO->post( qm, req, blobss.str() );
        m_reply = reply;
        fh_iostream oss = m_streamToQIO->getStream();
        return oss;
    }

    /******************************/
    /******************************/
    /******************************/
    
    class FerrisNetworkCookieJar
        :
        public QNetworkCookieJar
    {
        std::string m_dataPath;
        
    protected:
        QList<QNetworkCookie> allCookies () const;
        void setAllCookies ( const QList<QNetworkCookie> & cookieList );
    public:
        FerrisNetworkCookieJar( const std::string& dataPath, QObject * parent = 0 );
        virtual ~FerrisNetworkCookieJar();
        virtual QList<QNetworkCookie> cookiesForUrl ( const QUrl & url ) const;
        virtual bool setCookiesFromUrl ( const QList<QNetworkCookie> & cookieList, const QUrl & url );
    };

    FerrisNetworkCookieJar::FerrisNetworkCookieJar( const std::string& dataPath, QObject * parent )
        : m_dataPath( dataPath )
    {
        DEBUG << "FerrisNetworkCookieJar() path:" << m_dataPath << endl;
    }
    
    FerrisNetworkCookieJar::~FerrisNetworkCookieJar()
    {
    }

    QList<QNetworkCookie>
    FerrisNetworkCookieJar::cookiesForUrl ( const QUrl & url ) const
    {
        std::string earl = tostr(url);
        if( earl.rfind("?") != string::npos )
        {
            earl = earl.substr( 0, earl.rfind("?") );
        }
        
        DEBUG << "cookiesForUrl() url:" << earl << endl;
        
        std::stringstream ifs;
        QList<QNetworkCookie> cl;
        std::string bakedCookies = getConfigString( m_dataPath, earl, "" );
        if( bakedCookies.empty() )
        {
            DEBUG << "cookiesForUrl() bakedCookies is empty!" << endl;
            return cl;
        }
        ifs << bakedCookies;
        DEBUG << "cookiesForUrl() bakedCookies:" << bakedCookies << endl;

        int count = 0;
        boost::archive::text_iarchive archive( ifs );
        archive & count;
        DEBUG << "cookiesForUrl() count:" << count << " earl:" << earl << endl;
        for( int i=0; i<count; ++i )
        {
            int j;
            std::string s;
            std::string name;
            std::string value;
            archive & name;
            archive & value;
            QNetworkCookie c( name.c_str(), value.c_str() );
            archive & s; c.setDomain( s.c_str() );
            archive & s; c.setPath( s.c_str() );
            archive & j; c.setSecure(j);
            cl.push_back(c);

            DEBUG << "cookiesForUrl() c.domain:" << tostr(c.domain()) << endl;
            DEBUG << "cookiesForUrl() c.path:" << tostr(c.path()) << endl;
            DEBUG << "cookiesForUrl() c.name:" << tostr(c.name()) << endl;
        }

        DEBUG << "cookiesForUrl(end) cl.size:" << cl.size() << endl;
        return cl;
    }
    
    bool
    FerrisNetworkCookieJar::setCookiesFromUrl ( const QList<QNetworkCookie>& cookies, const QUrl & url )
    {
        std::string earl = tostr(url);
        if( earl.rfind("?") != string::npos )
        {
            earl = earl.substr( 0, earl.rfind("?") );
        }
        
        DEBUG << "setCookiesFromUrl() cookies.sz:" << cookies.size() << " url:" << earl << endl;

        std::stringstream ofs;
        boost::archive::text_oarchive archive( ofs );
        int sz = cookies.size();
        archive & sz;
        
        for( QList<QNetworkCookie>::const_iterator iter = cookies.begin(); iter != cookies.end(); ++iter )
        {
            QNetworkCookie c = *iter;
            DEBUG << "setCookiesFromUrl() c.domain:" << tostr(c.domain()) << endl;
            DEBUG << "setCookiesFromUrl() c.path:" << tostr(c.path()) << endl;
            DEBUG << "setCookiesFromUrl() c.name:" << tostr(c.name()) << endl;
            int i;
            string s;
            s = tostr(c.name());     archive & s;
            s = tostr(c.value());    archive & s;
            s = tostr(c.domain());   archive & s;
            s = tostr(c.path());     archive & s;
            i = c.isSecure();        archive & i;
        }
        DEBUG << "ofs:" << tostr(ofs) << endl;
        DEBUG << "m_dataPath:" << m_dataPath << endl;
        DEBUG << "setCookiesFromUrl() earl:" << earl << endl;
        
        setConfigString( m_dataPath, earl, tostr(ofs), true );
    }
    
    
    
    QList<QNetworkCookie>
    FerrisNetworkCookieJar::allCookies () const
    {
        DEBUG << "addCookies" << endl;
    }
    
    void
    FerrisNetworkCookieJar::setAllCookies ( const QList<QNetworkCookie> & cookieList )
    {
        DEBUG << "setAddCookies" << endl;
    }

    QNetworkAccessManager*
    getWikiQManager()
    {
        static QNetworkAccessManager* m_qmanager = 0;
        if( !m_qmanager )
        {
            m_qmanager = getQManager();

            // Shell::EnsureDB4("~/.ferris", "wiki-cookies.db");
            // FerrisNetworkCookieJar* cjar = new FerrisNetworkCookieJar("/.ferris/wiki-cookies.db");
            // m_qmanager = getQManager( cjar );
        }
        return m_qmanager;
    }
    
    

    
    /******************************/
    /******************************/
    /******************************/
    
    void loadCookies( QNetworkCookieJar* jar, const std::string& bakedCookies )
    {
        try
        {
            QList<QNetworkCookie> cl;

            std::stringstream ifs;
            ifs << bakedCookies;

            int count = 0;
            std::string earl;
            boost::archive::text_iarchive archive( ifs );
            archive & earl;
            archive & count;
            DEBUG << "loadCookies count:" << count << " earl:" << earl << endl;
            for( int i=0; i<count; ++i )
            {
                std::string value;
                archive & value;
                QNetworkCookie c;
                QByteArray valueba = toba(value);
                c.setValue( valueba );
                cl.push_back(c);
            }

            jar->setCookiesFromUrl( cl, QUrl( earl.c_str()));
        }
        catch( exception& e )
        {
        }
        
    }
    
    
    void
    Wiki::login()
    {
        DEBUG << "login()" << endl;
        
        userpass_t up = getPastebinUserPass( m_serverURL );
        DEBUG << "login() m_serverURL:" << m_serverURL
              << " user:" << up.first
              << " pass:" << up.second
              << endl;
        if( up.first.empty() || up.second.empty() )
        {
            cerr << "Warning, no username or password for the given wiki server" << endl
                 << "You can set one with the following command..." << endl
                 << " " << endl
                 << "ferris-capplet-auth --auth-service=wiki --auth-user=USER --auth-pass=PASS " << m_serverURL << endl
                 << endl;
        }

        string data;
        stringmap_t params;
        params.clear();
        params["action"] = "query";
        params["meta"]   = "userinfo";
        params["uiprop"] = "rights|hasmsg";
        data = request( params );
        DEBUG << "login(0) data:" << data << endl;
        string uid = getUID( data );
        if( !uid.empty() )
        {
            DEBUG << "checked before a login was attempted, have uid:" << uid << endl;
            DEBUG << " which means we are already in via a cookie." << endl;
            return;
        }
        

//        string bakedCookies = getConfigString( FDB_SECURE, "wiki-cookies", "" );
//        if( !bakedCookies.empty())
//            loadCookies( getQManager()->cookieJar(), bakedCookies );
        
        params.clear();
        params["action"]     = "login";
        params["lgname"]     = up.first;
        params["lgpassword"] = up.second;
        data = request( params );
        DEBUG << "login(1) data:" << data << endl;

        fh_domdoc doc = Factory::StringToDOM( data );
        DOMElement* root = doc->getDocumentElement();
        if( DOMElement* p = XML::getChildElement( root, "login" ) )
        {
            std::string result    = getAttribute( p, "result" );
            std::string token     = getAttribute( p, "token" );
            std::string sessionid = getAttribute( p, "sessionid" );
            std::string cookieprefix = getAttribute( p, "cookieprefix" );

            params["lgtoken"] = token;
            data = request( params );
            DEBUG << "login(2) data:" << data << endl;

            doc = Factory::StringToDOM( data );
            DOMElement* root = doc->getDocumentElement();
            if( DOMElement* p = XML::getChildElement( root, "login" ) )
            {
                std::string result    = getAttribute( p, "result" );
                DEBUG << "login(3) result:" << result << endl;
                if( result == "Success" )
                {
                    DEBUG << "login was OK..." << endl;
                    token     = getAttribute( p, "lgtoken" );
                    sessionid = getAttribute( p, "sessionid" );

                    {
                        params.clear();
                        params["action"] = "query";
                        params["meta"]   = "userinfo";
                        params["uiprop"] = "rights|hasmsg";
                        data = request( params );
                        DEBUG << "login(end) data:" << data << endl;
                        string uid = getUID( data );
                        DEBUG << "login(end) uid:" << uid << endl;
                        
                    }
                    

                    // // save cookies
                    // QNetworkAccessManager* qm = getQManager();
                    // QNetworkCookieJar* jar = qm->cookieJar();
                    // QList<QNetworkCookie> cookies = jar->cookiesForUrl( getAPIUrl() );
                    // DEBUG << "cookies.sz:" << cookies.size() << endl;

                    // stringlist_t stdcookies;
                    // for( QList<QNetworkCookie>::iterator iter = cookies.begin(); iter != cookies.end(); ++iter )
                    // {
                    //     QNetworkCookie c = *iter;
                    //     stdcookies.push_back( tostr(c.value()) );
                    // }
                    // DEBUG << "stdcookies.sz:" << stdcookies.size() << endl;


                    // std::stringstream ofs;
                    // boost::archive::text_oarchive archive( ofs );
                    // std::string earl = tostr(getAPIUrl());
                    // int sz = stdcookies.size();
                    // archive & earl;
                    // archive & sz;
                    // for( stringlist_t::iterator si = stdcookies.begin(); si!=stdcookies.end(); ++si )
                    // {
                    //     archive & *si;
                    // }
                    // DEBUG << "ofs:" << tostr(ofs) << endl;
                    // setConfigString( FDB_SECURE, "wiki-cookies", tostr(ofs) );
                    
                    
                }
            }
        }
        
    }

    std::list< std::string >
    Wiki::getPageList()
    {
        stringmap_t params;
        params["format"] = "xml";
//        params["project"] = "x";
//        params["page"]    = "x";

        QByteArray t1;
        string data = request( "http://paste.kde.org/api/xml/all", params, t1 );

        std::list< std::string > ret;
        fh_domdoc doc = Factory::StringToDOM( data );
        DOMElement* root = doc->getDocumentElement();
        DEBUG << "0..." << endl;
        if( DOMElement* q = XML::getChildElement( root, "pastes" ) )
        {
            DEBUG << "2" << endl;
            for( int i = 0; i < 10; ++i )
            {
                std::string pagename = "paste_" + tostr(i);
                
                if( DOMElement* p = XML::getChildElement( q, pagename ) )
                {
                    std::string id = XML::getChildText( p );
                    ret.push_back( id );
                }
            }
        }
        
        return ret;
    }

    std::string
    Wiki::move( const std::string& oldrdn, const std::string& newrdn )
    {
        ensureMoveToken();
        
        stringmap_t params;
        params["action"] = "move";
        params["from"]   = oldrdn;
        params["to"]     = newrdn;
        params["token"]  = m_moveToken;
        string data = request( params );

        DEBUG << "wiki::move() from:" << oldrdn
              << " to:" << newrdn
              << " token:" << m_moveToken
              << " data:" << data
              << endl;
        
        fh_domdoc doc = Factory::StringToDOM( data );
        DOMElement* root = doc->getDocumentElement();
        if( DOMElement* p = XML::getChildElement( root, "move" ) )
        {
            return "";
        }
        return "error";
    }
    
    
    
    void
    Wiki::handleFinished()
    {
        DEBUG << "handleFinished()" << endl;
        QNetworkReply* r = dynamic_cast<QNetworkReply*>(sender());
        m_waiter.unblock(r);
    }
    


    fh_Wiki getWiki( const std::string& serverURL )
    {
        typedef std::map< std::string, fh_Wiki > cache_t;
        static cache_t* cache = new cache_t;

        cache_t::iterator ci = cache->find( serverURL );
        if( ci != cache->end() )
        {
            return ci->second;
        }
        fh_Wiki w = new Wiki( serverURL );
//        w->login();
        cache->insert( make_pair( serverURL, w ));
        return w;
    }
    
    /*******************************************************************************/
    /*******************************************************************************/
    /*******************************************************************************/
    /*******************************************************************************/
    /*******************************************************************************/

    class FERRISEXP_CTXPLUGIN PastebinBaseContext
        :
        public FakeInternalContext
    {
        typedef FakeInternalContext _Base;

        
      protected:

        fh_context priv_getSubContext( const string& rdn ) throw( NoSuchSubContext )
        {
            try
            {
                DEBUG << "PastebinBaseContext::priv_getSubContext() p:" << getDirPath()
                      << " rdn:" << rdn
                      << endl;

                Items_t::iterator isSubContextBoundCache;
                if( priv_isSubContextBound( rdn, isSubContextBoundCache ) )
                {
                    DEBUG << "PastebinBaseContext::priv_getSubContext(bound already) p:" << getDirPath()
                          << " rdn:" << rdn
                          << endl;
                    return *isSubContextBoundCache;
                }
        
                bool created = false;
                {
                    DEBUG << "PastebinBaseContext::priv_getSubContext(2) p:" << getDirPath()
                          << " rdn:" << rdn
                          << endl;

                    fh_context ret = priv_readSubContext( rdn, created, false );
                    return ret;
                }
            }
            catch( NoSuchSubContext& e )
            {
                throw e;
            }
            catch( exception& e )
            {
                string s = e.what();
//            cerr << "NativeContext::priv_getSubContext() e:" << e.what() << endl;
                Throw_NoSuchSubContext( s, this );
            }
            catch(...)
            {}
            fh_stringstream ss;
            ss << "NoSuchSubContext:" << rdn;
            Throw_NoSuchSubContext( tostr(ss), this );
        }

        virtual void priv_read()
            {
                DEBUG << "PastebinBaseContext::priv_read(T) path:" << this->getDirPath() << endl;

                Context::EnsureStartStopReadingIsFiredRAII _raii1( this );
                this->clearContext();
                DEBUG << "priv_read(top) path:" << this->getDirPath() << endl;

                // PostgreSQLDBContext* cc = 0;
                // cc = priv_ensureSubContext( rdn, cc );
        
                DEBUG << "priv_read(done) path:" << this->getDirPath() << endl;
            }
        
      public:

        PastebinBaseContext( Context* parent, const std::string& rdn )
            :
            _Base( parent, rdn )
            {
            }
        
    };
    
    
    
    class FERRISEXP_CTXPLUGIN WikiFileContext
        :
        public StateLessEAHolder< WikiFileContext, PastebinBaseContext >
    {
        typedef StateLessEAHolder< WikiFileContext, PastebinBaseContext > _Base;
        typedef WikiFileContext _Self;

        
//        fh_Wiki m_wiki;
        fh_Wiki getWiki()
        {
            std::string serverURL = getParent()->getDirPath();
            fh_Wiki w = ::Ferris::getWiki( serverURL );
            return w;
        }
        
      protected:
        FakeInternalContext* priv_CreateContext( Context* parent, std::string rdn )
        {
            DEBUG << "wf priv_cc() dname:" << getDirName() << endl;
            
            WikiFileContext* ret = new WikiFileContext( parent, rdn );
            return ret;
        }

        bool m_haveProperties;
        int m_size;
        time_t m_mtime;
        
        time_t tt( const std::string& v )
        {
            struct tm z = Time::ParseTimeString( v );
            return Time::toTime(z);
        }
        
        void ensureProperties()
        {
            if( !m_haveProperties )
            {
                m_haveProperties = true;
                stringmap_t params;
                params["action"] = "query";
                params["titles"] = getDirName();
                params["prop"]   = "info";
                params["inprop"] = "protection|talkid";
                fh_Wiki w = getWiki();
                string data = w->request( params );
                DEBUG << "ensureProperties... data:" << data << endl;

                if( DOMElement* p = getQPP( Factory::StringToDOM( data )))
                {
                    m_size = toint(::Ferris::getAttribute( p, "length" ));
                    m_mtime = tt(::Ferris::getAttribute( p, "touched" ));

                    DEBUG << "sz:" << m_size << endl;
                    DEBUG << "mtime:" << Time::toTimeString(m_mtime) << endl;
                    
                }
            }
        }
        
        
      public:

        fh_iostream getSizeStream( Context*, const std::string&, EA_Atom* attr )
        {
            ensureProperties();
            fh_stringstream ss;
            ss << m_size;
            return ss;
        }
        fh_iostream getMTimeStream( Context*, const std::string&, EA_Atom* attr )
        {
            ensureProperties();
            fh_stringstream ss;
            ss << m_mtime;
            return ss;
        }
        
        WikiFileContext( Context* parent, const std::string& rdn )
            : _Base( parent, rdn )
            , m_haveProperties(false)
            , m_size(0)
            {
                DEBUG << "WikiFileContext() rdn:" << rdn << endl;
                static Util::SingleShot virgin;
                addAttribute( "size",  this, &_Self::getSizeStream, XSD_BASIC_INT );
                addAttribute( "mtime", this, &_Self::getMTimeStream, XSD_BASIC_INT );
                supplementStateLessAttributes_size("size");
                supplementStateLessAttributes_timet("mtime");
                if( virgin() )
                    createStateLessAttributes( true );

            }

        virtual bool isPrivate()
        {
            bool ret = false;
            Context* c = getParent();
            while( c )
            {
                if( c->getDirName() == "private" )
                {
                    ret = true;
                    break;
                }
                if( !c->isParentBound() )
                    break;
                c = c->getParent();
            }
            return ret;
        }
        virtual std::string getPassword()
        {
            std::string ret = "";
            if( isPrivate() )
            {
                Context* c = this;
                while( c && c->isParentBound() )
                {
                    if( c->getParent()->getDirName() == "private" )
                    {
                        ret = c->getDirName();
                        break;
                    }
                    if( !c->isParentBound() )
                        break;
                    c = c->getParent();
                }
            }
            return ret;
        }
        
        
        virtual bool isDir()
        {
            return false;
        }

        ferris_ios::openmode
        getSupportedOpenModes()
            {
                return ios::in | ios::out | ferris_ios::o_mseq | ios::trunc | ios::binary;
            }

        void
        OnStreamingWriteClosed( fh_istream& ss, std::streamsize tellp, ferris_ios::openmode m )
            {
                cerr << "OnStreamingWriteClosed()" << endl;

                if( !(m & std::ios::out) )
                    return;

                fh_Wiki w = getWiki();
                w->streamingUploadComplete();
            }

        fh_istream
            priv_getIStream( ferris_ios::openmode m )
            throw (FerrisParentNotSetError,
                   AttributeNotWritable,
                   CanNotGetStream,
                   std::exception)
            {
                DEBUG << "priv_getIStream()" << endl;
                fh_stringstream ss;
//                return ss;

                stringstream earlss;
                earlss << "http://paste.kde.org/api/raw/" << getDirName();
                DEBUG << "redirecting to URL:" << tostr(earlss) << endl;
                fh_context c = Resolve(tostr(earlss));
                return c->getIStream();
            }
        
        fh_iostream
            priv_getIOStream( ferris_ios::openmode m )
            throw (FerrisParentNotSetError,
                   AttributeNotWritable,
                   CanNotGetStream,
                   std::exception)
            {
                DEBUG << "priv_getIOStream()" << endl;

                int ContentLength = 0;
                string title = getDirName();
                string   rdn = getDirName();
                fh_Wiki    w = getWiki();
                DEBUG << "priv_getIOStream(2)" << endl;

                fh_iostream ret = w->getEditIOStream( ContentLength, title, getPassword(), isPrivate() );
                ret->getCloseSig().connect(
                    boost::bind( &_Self::OnStreamingWriteClosed, this, _1, _2, m )); 
                DEBUG << "priv_getIOStream(2e)" << endl;
                return ret;
            }
        
        
    };
    
    /*******************************************************************************/
    /*******************************************************************************/
    /*******************************************************************************/
    
    class FERRISEXP_CTXPLUGIN WikiContext
        :
        public StateLessEAHolder< WikiContext, PastebinBaseContext >
    {
        typedef StateLessEAHolder< WikiContext, PastebinBaseContext > _Base;
        
      protected:

        FakeInternalContext* priv_CreateContext( Context* parent, std::string rdn )
        {
            DEBUG << "wc::priv_cc() dname:" << getDirName() << " rdn:" << rdn << endl;
            
            if( getDirName() == "new" || getDirName() == "private" )
            {
                WikiFileContext* ret = new WikiFileContext( parent, rdn );
                return ret;
            }
            
            WikiContext* ret = new WikiContext( parent, rdn );
            return ret;
        }

        virtual void priv_FillCreateSubContextSchemaParts( CreateSubContextSchemaPart_t& m )
            {
                DEBUG << "upload dir. setting file creation schema" << endl;
                m["file"] = SubContextCreator(SL_SubCreate_file,
                                              "	<elementType name=\"file\">\n"
                                              "		<elementType name=\"name\" default=\"newfile\">\n"
                                              "			<dataTypeRef name=\"string\"/>\n"
                                              "		</elementType>\n"
                                              "	</elementType>\n");
            }

        virtual fh_context SubCreate_file( fh_context c, fh_context md )
            {
                string rdn = getStrSubCtx( md, "name", "" );
                DEBUG << "create_file for rdn:" << rdn << endl;

                fh_context child = 0;
                child = new WikiFileContext( this, rdn );
                Insert( GetImpl(child), false, true );

                DEBUG << "create_file OK for rdn:" << rdn << endl;
                return child;
            }


        fh_Wiki getWiki()
        {
            std::string serverURL = getDirPath();
            fh_Wiki w = ::Ferris::getWiki( serverURL );
            return w;
        }
        
        
        fh_iostream
            priv_getIOStream( ferris_ios::openmode m )
            throw (FerrisParentNotSetError,
                   AttributeNotWritable,
                   CanNotGetStream,
                   std::exception)
            {
                DEBUG << "wc::priv_getIOStream()" << endl;
                return _Base::priv_getIOStream(m);
            }
        
      public:

        virtual void priv_read()
            {
                DEBUG << "WikiContext::priv_read(T) path:" << this->getDirPath() << endl;

                if( getDirName() == "index.php" )
                {
                    DEBUG << "WikiContext::priv_read(T) index path:" << this->getDirPath() << endl;
                    DEBUG << "WikiContext::priv_read(T)    count:" << getSubContextCount() << endl;
                    DEBUG << "WikiContext::priv_read(T) haveread:" << getHaveReadDir() << endl;
                    
                    Context::EnsureStartStopReadingIsFiredRAII _raii1( this );

                    if( getSubContextCount() && getHaveReadDir() )
                    {
                        emitExistsEventForEachItem();
                        return;
                    }

                    // fh_Wiki w = getWiki();
                    // DEBUG << "getting page list................" << endl;
                    // WikiPageList_t pages = w->getPageList();
                    // DEBUG << "pages.sz:" << pages.size() << endl;

                    // for( WikiPageList_t::iterator iter = pages.begin(); iter!=pages.end(); ++iter )
                    // {
                    //     WikiFileContext* cc = 0;
                    //     std::string rdn = iter->getName();
                    //     cc = priv_ensureSubContext( rdn, cc );
                    // }

                    return;
                }
                DEBUG << "have read subdirs..." << endl;
                
                _Base::priv_read();
            }
        
        WikiContext( Context* parent, const std::string& rdn )
            :
            _Base( parent, rdn )
            {
                DEBUG << "WikiContext() rdn:" << rdn << endl;
                static Util::SingleShot virgin;
                if( virgin() )
                    createStateLessAttributes( true );

            }

        virtual ~WikiContext()
            {
                DEBUG << "~WikiContext() rdn:" << getDirName() << endl;
            }
        
    };


    
    /********************************************************************************/
    /********************************************************************************/
    /*******************************************************************************/

    class FERRISEXP_CTXPLUGIN WikiListContext
        :
        public StateLessEAHolder< WikiListContext, PastebinBaseContext >
    {
        typedef StateLessEAHolder< WikiListContext, PastebinBaseContext > _Base;
        
      protected:

        FakeInternalContext* priv_CreateContext( Context* parent, std::string rdn )
        {
            WikiFileContext* ret = new WikiFileContext( parent, rdn );
            return ret;
        }


        fh_Wiki getWiki()
        {
            std::string serverURL = getDirPath();
            fh_Wiki w = ::Ferris::getWiki( serverURL );
            return w;
        }
        
      public:

        virtual void priv_read()
            {
                DEBUG << "WikiListContext::priv_read(T) path:" << this->getDirPath() << endl;
                Context::EnsureStartStopReadingIsFiredRAII _raii1( this );
                clearContext();

                fh_Wiki w = getWiki();
                DEBUG << "getting page list................" << endl;
                std::list< std::string > pages = w->getPageList();
                DEBUG << "pages.sz:" << pages.size() << endl;
                
                for( std::list< std::string >::iterator iter = pages.begin(); iter!=pages.end(); ++iter )
                {
                    WikiFileContext* cc = 0;
                    std::string rdn = *iter;
                    cc = priv_ensureSubContext( rdn, cc );
                }
                
                return;
            }
        
        WikiListContext( Context* parent, const std::string& rdn )
            :
            _Base( parent, rdn )
            {
                DEBUG << "WikiListContext() rdn:" << rdn << endl;
                static Util::SingleShot virgin;
                if( virgin() )
                    createStateLessAttributes( true );

            }

        virtual ~WikiListContext()
            {
                DEBUG << "~WikiListContext() rdn:" << getDirName() << endl;
            }
        
    };

    /********************************************************************************/
    /********************************************************************************/
    /********************************************************************************/

    class FERRISEXP_CTXPLUGIN PastebinServerContext
        :
        public StateLessEAHolder< WikiContext, PastebinBaseContext >
    {
        typedef PastebinServerContext _Self;
        typedef StateLessEAHolder< WikiContext, PastebinBaseContext > _Base;

        
    public:
        
        PastebinServerContext( Context* parent, const std::string& rdn )
            :
            _Base( parent, rdn )
            {
                DEBUG << "PastebinServerContext() rdn:" << rdn << endl;
                createStateLessAttributes();

                {
                    WikiListContext* cc = 0;
                    cc = priv_ensureSubContext( "list", cc );
                }

                {
                    WikiFileContext* cc = 0;
                    cc = priv_ensureSubContext( "private", cc );
                    cc = priv_ensureSubContext( "new", cc );
                }
                
            }

        virtual ~PastebinServerContext()
            {
                DEBUG << "~PastebinServerContext() rdn:" << getDirName() << endl;
            }
        
        
    protected:

        FakeInternalContext* priv_CreateContext( Context* parent, std::string rdn )
        {
            WikiContext* ret = new WikiContext( parent, rdn );
            return ret;
        }
        virtual void priv_read()
            {
                DEBUG << "PastebinServerContext::priv_read(T) path:" << this->getDirPath() << endl;
                Context::EnsureStartStopReadingIsFiredRAII _raii1( this );
                emitExistsEventForEachItem();
            }
        
    };

    /*******************************************************************************/
    /*******************************************************************************/
    /*******************************************************************************/
    
    class FERRISEXP_CTXPLUGIN PastebinRootContext
        :
        public networkRootContext< PastebinServerContext >
    {
        typedef networkRootContext< PastebinServerContext > _Base;
        
    public:
        
        PastebinRootContext( Context* parent,
                             const std::string& rdn,
                             bool bindall = false )
            :
            _Base( parent, rdn, bindall )
            {
                createStateLessAttributes();
                tryAddHost( "kde.org" );
            }
        
    protected:

        virtual void createStateLessAttributes( bool force = false )
            {
                DEBUG << "RootContext::createStateLessAttributes() " << endl;
                static Util::SingleShot virgin;
                if( virgin() )
                {
                    _Base::createStateLessAttributes( true );
                    this->supplementStateLessAttributes( true );
                }
            }

        /* We want to make sure that this object is always around */
        enum {
            REFCOUNT = 3
        };
        virtual Handlable::ref_count_t AddRef()
            { return REFCOUNT; }
        virtual Handlable::ref_count_t Release()
            { return REFCOUNT; }
        
        virtual std::string TryToCheckServerExists( const std::string& rdn )
            {
                DEBUG << "TryToCheckServerExists() rdn:" << rdn << endl;
                
                if( rdn == "localhost" )
                    return "";
                
                return _Base::TryToCheckServerExists( rdn );
            }
    };
    
    /*******************************************************************************/
    /*******************************************************************************/
    /*******************************************************************************/
    /*******************************************************************************/
    /*******************************************************************************/
    /*******************************************************************************/
    /*******************************************************************************/
    /*******************************************************************************/
    /*******************************************************************************/
    /*******************************************************************************/
    /*******************************************************************************/
    /*******************************************************************************/
    /*******************************************************************************/
    /*******************************************************************************/
    /*******************************************************************************/
    
    extern "C"
    {
        fh_context Brew( RootContextFactory* rf )
            throw( RootContextCreationFailed )
        {
            try
            {
                ensureQApplication();
                DEBUG << "Brew()" << endl;

                static PastebinRootContext* c = 0;
                const string& root = rf->getInfo( RootContextFactory::ROOT );

                if( !c )
                {
                    DEBUG << "making root context " << endl;
                    c = new PastebinRootContext(0, "/", false );

                    DEBUG << "Adding localhosts to root context " << endl;
//                    c->tryAugmentLocalhostNames();
                
                    // Bump ref count.
                    static fh_context keeper = c;
                    static fh_context keeper2 = keeper;
                }

                fh_context ret = c;
                return ret;
            }
            catch( exception& e )
            {
                LG_PG_W << "e:" << e.what() << endl;
                Throw_RootContextCreationFailed( e.what(), 0 );
            }
        }
    }

};

