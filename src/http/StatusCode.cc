#include "squid.h"
#include "Debug.h"
#include "http/StatusCode.h"

const char *
Http::StatusCodeString(const Http::StatusCode status)
{
    switch (status) {

        // 000
    case Http::scNone:
        return "Init";		/* we init .status with code 0 */
        break;

        // 100-199
    case Http::scContinue:
        return "Continue";
        break;

    case Http::scSwitchingProtocols:
        return "Trocando Protocolos";
        break;

    case Http::scProcessing:
        return "Processando";
        break;

        // 200-299
    case Http::scOkay:
        return "<font color='@9933FF'>Proxy Conectado</font>";
        break;

    case Http::scCreated:
        return "Criado";
        break;

    case Http::scAccepted:
        return "Aceito";
        break;

    case Http::scNonAuthoritativeInformation:
        return "Non-Authoritative Information";
        break;

    case Http::scNoContent:
        return "No Content";
        break;

    case Http::scResetContent:
        return "Reset Content";
        break;

    case Http::scPartialContent:
        return "Partial Content";
        break;

    case Http::scMultiStatus:
        return "Multi-Status";
        break;

    case Http::scAlreadyReported:
        return "Already Reported";
        break;

    case Http::scImUsed:
        return "IM Used";
        break;

        // 300-399
    case Http::scMultipleChoices:
        return "Multiple Choices";
        break;

    case Http::scMovedPermanently:
        return "Movido Permanentemente";
        break;

    case Http::scFound:
        return "Encontrado";
        break;

    case Http::scSeeOther:
        return "See Other";
        break;

    case Http::scNotModified:
        return "Not Modified";
        break;

    case Http::scUseProxy:
        return "Proxy em uso";
        break;

    case Http::scTemporaryRedirect:
        return "Temporary Redirect";
        break;

    case Http::scPermanentRedirect:
        return "Permanent Redirect";
        break;

        // 400-499
    case Http::scBadRequest:
        return "Requisição Inválida";
        break;

    case Http::scUnauthorized:
        return "Não Autorizado";
        break;

    case Http::scPaymentRequired:
        return "Payment Required";
        break;

    case Http::scForbidden:
        return "Conexão Recusada";
        break;

    case Http::scNotFound:
        return "Não Encontrado";
        break;

    case Http::scMethodNotAllowed:
        return "Method Not Allowed";
        break;

    case Http::scNotAcceptable:
        return "Not Acceptable";
        break;

    case Http::scProxyAuthenticationRequired:
        return "<font color='red'>O Proxy requer autenticação</font>";
        break;

    case Http::scRequestTimeout:
        return "Request Timeout";
        break;

    case Http::scConflict:
        return "Conflict";
        break;

    case Http::scGone:
        return "Gone";
        break;

    case Http::scLengthRequired:
        return "Length Required";
        break;

    case Http::scPreconditionFailed:
        return "Falha na Precondição";
        break;

    case Http::scPayloadTooLarge:
        return "Payload Muito Grande";
        break;

    case Http::scUriTooLong:
        return "URI Muito Longa";
        break;

    case Http::scUnsupportedMediaType:
        return "Tipo não suportad";
        break;

    case Http::scRequestedRangeNotSatisfied:
        return "Requested Range Not Satisfiable";
        break;

    case Http::scExpectationFailed:
        return "Expectation Failed";
        break;

    case Http::scUnprocessableEntity:
        return "Unprocessable Entity";
        break;

    case Http::scLocked:
        return "Bloqueado";
        break;

    case Http::scFailedDependency:
        return "A Dependência falhou";
        break;

    case Http::scUpgradeRequired:
        return "Atualização Requerida";
        break;

    case Http::scPreconditionRequired:
        return "Precondição Requerida";
        break;

    case Http::scTooManyRequests:
        return "Muitas Solicitações";
        break;

    case Http::scRequestHeaderFieldsTooLarge:
        return "Request Header Fields Too Large";
        break;

        // 500-599
    case Http::scInternalServerError:
        return "Erro Interno no Servidor";
        break;

    case Http::scNotImplemented:
        return "Nâo Disponível";
        break;

    case Http::scBadGateway:
        return "Erro na Gateway";
        break;

    case Http::scServiceUnavailable:
        return "Serviço Indisponível";
        break;

    case Http::scGatewayTimeout:
        return "Gateway Offline";
        break;

    case Http::scHttpVersionNotSupported:
        return "Versão HTTP não suportada";
        break;

    case Http::scVariantAlsoNegotiates:
        return "Variant Also Negotiates";
        break;

    case Http::scInsufficientStorage:
        return "Armazenamento Cheio";
        break;

    case Http::scLoopDetected:
        return "Loop Detectado";
        break;

    case Http::scNotExtended:
        return "Not Extended";
        break;

    case Http::scNetworkAuthenticationRequired:
        return "A Rede requer Autenticação";
        break;

        // 600+
    case Http::scInvalidHeader:
    case Http::scHeaderTooLarge:
        // fall through to default.

    default:
        debugs(57, 3, "Unassigned HTTP status code: " << status);
    }
    return "Unassigned";
}
