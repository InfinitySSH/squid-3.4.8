#include "squid.h"
#include "auth/ntlm/auth_ntlm.h"
#include "auth/ntlm/UserRequest.h"
#include "auth/State.h"
#include "cbdata.h"
#include "client_side.h"
#include "globals.h"
#include "HttpRequest.h"
#include "SquidTime.h"

Auth::Ntlm::UserRequest::UserRequest()
{
    waiting=0;
    client_blob=0;
    server_blob=0;
    authserver=NULL;
    request=NULL;
}

Auth::Ntlm::UserRequest::~UserRequest()
{
    assert(LockCount()==0);
    safe_free(server_blob);
    safe_free(client_blob);

    releaseAuthServer();

    if (request) {
        HTTPMSGUNLOCK(request);
        request = NULL;
    }
}

const char *
Auth::Ntlm::UserRequest::connLastHeader()
{
    return NULL;
}

int
Auth::Ntlm::UserRequest::authenticated() const
{
    if (user() != NULL && user()->credentials() == Auth::Ok) {
        debugs(29, 9, HERE << "user authenticated.");
        return 1;
    }

    debugs(29, 9, HERE << "user not fully authenticated.");
    return 0;
}

Auth::Direction
Auth::Ntlm::UserRequest::module_direction()
{
    /* null auth_user is checked for by Auth::UserRequest::direction() */

    if (waiting || client_blob)
        return Auth::CRED_LOOKUP; /* need helper response to continue */

    if (user()->auth_type != Auth::AUTH_NTLM)
        return Auth::CRED_ERROR;

    switch (user()->credentials()) {

    case Auth::Handshake:
        assert(server_blob);
        return Auth::CRED_CHALLENGE;

    case Auth::Ok:
        return Auth::CRED_VALID;

    case Auth::Failed:
        return Auth::CRED_ERROR; // XXX: really? not VALID or CHALLENGE?

    default:
        debugs(29, DBG_IMPORTANT, "WARNING: NTLM Authentication in unexpected state: " << user()->credentials());
        return Auth::CRED_ERROR;
    }
}

void
Auth::Ntlm::UserRequest::module_start(AUTHCB * handler, void *data)
{
    static char buf[MAX_AUTHTOKEN_LEN];

    assert(data);
    assert(handler);

    if (static_cast<Auth::Ntlm::Config*>(Auth::Config::Find("ntlm"))->authenticateProgram == NULL) {
        debugs(29, DBG_CRITICAL, "ERROR: NTLM Start: no NTLM program configured.");
        handler(data);
        return;
    }

    debugs(29, 8, HERE << "credentials state is '" << user()->credentials() << "'");

    if (user()->credentials() == Auth::Pending) {
        snprintf(buf, sizeof(buf), "YR %s\n", client_blob); //CHECKME: can ever client_blob be 0 here?
    } else {
        snprintf(buf, sizeof(buf), "KK %s\n", client_blob);
    }

    waiting = 1;

    safe_free(client_blob);
    helperStatefulSubmit(ntlmauthenticators, buf, Auth::Ntlm::UserRequest::HandleReply,
                         new Auth::StateData(this, handler, data), authserver);
}

/**
 * Atomic action: properly release the NTLM auth helpers which may have been reserved
 * for this request connections use.
 */
void
Auth::Ntlm::UserRequest::releaseAuthServer()
{
    if (authserver) {
        debugs(29, 6, HERE << "releasing NTLM auth server '" << authserver << "'");
        helperStatefulReleaseServer(authserver);
        authserver = NULL;
    } else
        debugs(29, 6, HERE << "No NTLM auth server to release.");
}

void
Auth::Ntlm::UserRequest::authenticate(HttpRequest * aRequest, ConnStateData * conn, http_hdr_type type)
{
    assert(this);

    /* Check that we are in the client side, where we can generate
     * auth challenges */

    if (conn == NULL || !cbdataReferenceValid(conn)) {
        user()->credentials(Auth::Failed);
        debugs(29, DBG_IMPORTANT, "WARNING: NTLM Authentication attempt to perform authentication without a connection!");
        return;
    }

    if (waiting) {
        debugs(29, DBG_IMPORTANT, "WARNING: NTLM Authentication waiting for helper reply!");
        return;
    }

    if (server_blob) {
        debugs(29, 2, HERE << "need to challenge client '" << server_blob << "'!");
        return;
    }

    /* get header */
    const char *proxy_auth = aRequest->header.getStr(type);

    /* locate second word */
    const char *blob = proxy_auth;

    /* if proxy_auth is actually NULL, we'd better not manipulate it. */
    if (blob) {
        while (xisspace(*blob) && *blob)
            ++blob;

        while (!xisspace(*blob) && *blob)
            ++blob;

        while (xisspace(*blob) && *blob)
            ++blob;
    }

    switch (user()->credentials()) {

    case Auth::Unchecked:
        /* we've received a ntlm request. pass to a helper */
        debugs(29, 9, HERE << "auth state ntlm none. Received blob: '" << proxy_auth << "'");
        user()->credentials(Auth::Pending);
        safe_free(client_blob);
        client_blob=xstrdup(blob);
        assert(conn->getAuth() == NULL);
        conn->setAuth(this, "new NTLM handshake request");
        request = aRequest;
        HTTPMSGLOCK(request);
        break;

    case Auth::Pending:
        debugs(29, DBG_IMPORTANT, HERE << "need to ask helper");
        break;

    case Auth::Handshake:
        /* we should have received a blob from the client. Hand it off to
         * some helper */
        safe_free(client_blob);
        client_blob = xstrdup(blob);
        if (request)
            HTTPMSGUNLOCK(request);
        request = aRequest;
        HTTPMSGLOCK(request);
        break;

    case Auth::Ok:
        fatal("Auth::Ntlm::UserRequest::authenticate: unexpect auth state DONE! Report a bug to the squid developers.\n");
        break;

    case Auth::Failed:
        /* we've failed somewhere in authentication */
        debugs(29, 9, HERE << "auth state ntlm failed. " << proxy_auth);
        break;
    }
}

void
Auth::Ntlm::UserRequest::HandleReply(void *data, const HelperReply &reply)
{
    Auth::StateData *r = static_cast<Auth::StateData *>(data);

    debugs(29, 8, HERE << "helper: '" << reply.whichServer << "' sent us reply=" << reply);

    if (!cbdataReferenceValid(r->data)) {
        debugs(29, DBG_IMPORTANT, "ERROR: NTLM Authentication invalid callback data. helper '" << reply.whichServer << "'.");
        delete r;
        return;
    }

    Auth::UserRequest::Pointer auth_user_request = r->auth_user_request;
    assert(auth_user_request != NULL);

    // add new helper kv-pair notes to the credentials object
    // so that any transaction using those credentials can access them
    auth_user_request->user()->notes.appendNewOnly(&reply.notes);

    Auth::Ntlm::UserRequest *lm_request = dynamic_cast<Auth::Ntlm::UserRequest *>(auth_user_request.getRaw());
    assert(lm_request != NULL);
    assert(lm_request->waiting);

    lm_request->waiting = 0;
    safe_free(lm_request->client_blob);

    assert(auth_user_request->user() != NULL);
    assert(auth_user_request->user()->auth_type == Auth::AUTH_NTLM);

    if (lm_request->authserver == NULL)
        lm_request->authserver = reply.whichServer.get(); // XXX: no locking?
    else
        assert(reply.whichServer == lm_request->authserver);

    switch (reply.result) {
    case HelperReply::TT:
        /* we have been given a blob to send to the client */
        safe_free(lm_request->server_blob);
        lm_request->request->flags.mustKeepalive = true;
        if (lm_request->request->flags.proxyKeepalive) {
            const char *serverBlob = reply.notes.findFirst("token");
            lm_request->server_blob = xstrdup(serverBlob);
            auth_user_request->user()->credentials(Auth::Handshake);
            auth_user_request->denyMessage("Authentication in progress");
            debugs(29, 4, HERE << "Need to challenge the client with a server token: '" << serverBlob << "'");
        } else {
            auth_user_request->user()->credentials(Auth::Failed);
            auth_user_request->denyMessage("NTLM authentication requires a persistent connection");
        }
        break;

    case HelperReply::Okay: {
        /* we're finished, release the helper */
        const char *userLabel = reply.notes.findFirst("user");
        if (!userLabel) {
            auth_user_request->user()->credentials(Auth::Failed);
            safe_free(lm_request->server_blob);
            lm_request->releaseAuthServer();
            debugs(29, DBG_CRITICAL, "ERROR: NTLM Authentication helper returned no username. Result: " << reply);
            break;
        }
        auth_user_request->user()->username(userLabel);
        auth_user_request->denyMessage("Login successful");
        safe_free(lm_request->server_blob);
        lm_request->releaseAuthServer();

        debugs(29, 4, HERE << "Successfully validated user via NTLM. Username '" << userLabel << "'");
        /* connection is authenticated */
        debugs(29, 4, HERE << "authenticated user " << auth_user_request->user()->username());
        /* see if this is an existing user with a different proxy_auth
         * string */
        AuthUserHashPointer *usernamehash = static_cast<AuthUserHashPointer *>(hash_lookup(proxy_auth_username_cache, auth_user_request->user()->username()));
        Auth::User::Pointer local_auth_user = lm_request->user();
        while (usernamehash && (usernamehash->user()->auth_type != Auth::AUTH_NTLM ||
                                strcmp(usernamehash->user()->username(), auth_user_request->user()->username()) != 0))
            usernamehash = static_cast<AuthUserHashPointer *>(usernamehash->next);
        if (usernamehash) {
            /* we can't seamlessly recheck the username due to the
             * challenge-response nature of the protocol.
             * Just free the temporary auth_user after merging as
             * much of it new state into the existing one as possible */
            usernamehash->user()->absorb(local_auth_user);
            /* from here on we are working with the original cached credentials. */
            local_auth_user = usernamehash->user();
            auth_user_request->user(local_auth_user);
        } else {
            /* store user in hash's */
            local_auth_user->addToNameCache();
        }
        /* set these to now because this is either a new login from an
         * existing user or a new user */
        local_auth_user->expiretime = current_time.tv_sec;
        auth_user_request->user()->credentials(Auth::Ok);
        debugs(29, 4, HERE << "Successfully validated user via NTLM. Username '" << auth_user_request->user()->username() << "'");
    }
    break;

    case HelperReply::Error: {
        /* authentication failure (wrong password, etc.) */
        const char *errNote = reply.notes.find("message");
        if (errNote != NULL)
            auth_user_request->denyMessage(errNote);
        else
            auth_user_request->denyMessage("NTLM Authentication denied with no reason given");
        auth_user_request->user()->credentials(Auth::Failed);
        safe_free(lm_request->server_blob);
        lm_request->releaseAuthServer();
        debugs(29, 4, "Failed validating user via NTLM. Result: " << reply);
    }
    break;

    case HelperReply::Unknown:
        debugs(29, DBG_IMPORTANT, "ERROR: NTLM Authentication Helper '" << reply.whichServer << "' crashed!.");
        /* continue to the next case */

    case HelperReply::BrokenHelper: {
        /* TODO kick off a refresh process. This can occur after a YR or after
         * a KK. If after a YR release the helper and resubmit the request via
         * Authenticate NTLM start.
         * If after a KK deny the user's request w/ 407 and mark the helper as
         * Needing YR. */
        const char *errNote = reply.notes.find("message");
        if (reply.result == HelperReply::Unknown)
            auth_user_request->denyMessage("Internal Error");
        else if (errNote != NULL)
            auth_user_request->denyMessage(errNote);
        else
            auth_user_request->denyMessage("NTLM Authentication failed with no reason given");
        auth_user_request->user()->credentials(Auth::Failed);
        safe_free(lm_request->server_blob);
        lm_request->releaseAuthServer();
        debugs(29, DBG_IMPORTANT, "ERROR: NTLM Authentication validating user. Result: " << reply);
    }
    break;
    }

    if (lm_request->request) {
        HTTPMSGUNLOCK(lm_request->request);
        lm_request->request = NULL;
    }
    r->handler(r->data);
    delete r;
}
