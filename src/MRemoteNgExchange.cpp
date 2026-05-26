#include "MRemoteNgExchange.h"

#include "CredentialVault.h"
#include "Util.h"

#include <msxml6.h>

#include <algorithm>
#include <fstream>
#include <sstream>

#pragma comment(lib, "msxml6.lib")
#pragma comment(lib, "oleaut32.lib")

namespace {

std::wstring VariantToString(const VARIANT& v) {
    if (v.vt == VT_BSTR && v.bstrVal) {
        return std::wstring(v.bstrVal);
    }
    return L"";
}

std::wstring GetAttr(IXMLDOMElement* element, const wchar_t* name) {
    if (!element) {
        return L"";
    }
    VARIANT v{};
    VariantInit(&v);
    BSTR attrName = SysAllocString(name);
    const HRESULT hr = element->getAttribute(attrName, &v);
    SysFreeString(attrName);
    if (FAILED(hr)) {
        return L"";
    }
    const std::wstring result = VariantToString(v);
    VariantClear(&v);
    return result;
}

int GetAttrInt(IXMLDOMElement* element, const wchar_t* name, int def) {
    const std::wstring s = GetAttr(element, name);
    if (s.empty()) {
        return def;
    }
    try {
        return std::stoi(s);
    } catch (...) {
        return def;
    }
}

void ParseUsername(const std::wstring& userField, const std::wstring& domainField, std::wstring& username,
                   std::wstring& domain) {
    username = userField;
    domain = domainField;
    if (!username.empty() && domain.empty()) {
        const size_t slash = username.find(L'\\');
        if (slash != std::wstring::npos) {
            domain = username.substr(0, slash);
            username = username.substr(slash + 1);
        }
    }
}

void ParseHostPort(const std::wstring& hostname, int portAttr, std::wstring& host, int& port) {
    host = hostname;
    port = portAttr > 0 ? portAttr : 3389;
    const size_t colon = hostname.rfind(L':');
    if (colon == std::wstring::npos || colon == 0) {
        return;
    }
    const std::wstring tail = hostname.substr(colon + 1);
    if (tail.empty() || !std::all_of(tail.begin(), tail.end(), iswdigit)) {
        return;
    }
    host = hostname.substr(0, colon);
    port = std::stoi(tail);
}

std::wstring FindOrCreateCredential(ConnectionTreeModel& model, const std::wstring& username,
                                    const std::wstring& domain) {
    if (username.empty() && domain.empty()) {
        return L"";
    }
    for (const auto& c : model.Credentials()) {
        if (c.username == username && c.domain == domain) {
            return c.id;
        }
    }
    std::wstring label = domain.empty() ? username : domain + L"\\" + username;
    CredentialMeta* meta = model.AddCredential(label, username, domain);
    return meta ? meta->id : L"";
}

std::wstring FormatUsernameForExport(const std::wstring& username, const std::wstring& domain) {
    if (username.empty()) {
        return L"";
    }
    if (domain.empty()) {
        return username;
    }
    if (username.find(L'\\') != std::wstring::npos || username.find(L'@') != std::wstring::npos) {
        return username;
    }
    return domain + L"\\" + username;
}

std::wstring XmlEscapeAttr(const std::wstring& value) {
    std::wstring out;
    out.reserve(value.size() + 8);
    for (wchar_t c : value) {
        switch (c) {
            case L'&':
                out += L"&amp;";
                break;
            case L'"':
                out += L"&quot;";
                break;
            case L'\'':
                out += L"&apos;";
                break;
            case L'<':
                out += L"&lt;";
                break;
            case L'>':
                out += L"&gt;";
                break;
            default:
                out += c;
                break;
        }
    }
    return out;
}

// Shared mRemoteNG node attribute block (ConfVersion 2.6 style) with variable fields substituted.
std::wstring BuildNodeOpenTag(const std::wstring& name, const std::wstring& type, const std::wstring& id,
                              const std::wstring& username, const std::wstring& domain, const std::wstring& password,
                              const std::wstring& hostname, int port, const std::wstring& protocol) {
    std::wostringstream tag;
    tag << L"<Node Name=\"" << XmlEscapeAttr(name) << L"\" Type=\"" << XmlEscapeAttr(type)
        << L"\" Expanded=\"true\" Descr=\"\" Icon=\"mRemoteNG\" Panel=\"General\" Id=\"" << XmlEscapeAttr(id)
        << L"\" Username=\"" << XmlEscapeAttr(username) << L"\" Domain=\"" << XmlEscapeAttr(domain)
        << L"\" Password=\"" << XmlEscapeAttr(password) << L"\" Hostname=\"" << XmlEscapeAttr(hostname)
        << L"\" Protocol=\"" << XmlEscapeAttr(protocol) << L"\" PuttySession=\"Default Settings\" Port=\""
        << port
        << L"\" ConnectToConsole=\"false\" UseCredSsp=\"true\" RenderingEngine=\"IE\" "
           L"ICAEncryptionStrength=\"EncrBasic\" RDPAuthenticationLevel=\"NoAuth\" RDPMinutesToIdleTimeout=\"0\" "
           L"RDPAlertIdleTimeout=\"false\" LoadBalanceInfo=\"\" Colors=\"Colors16Bit\" Resolution=\"FitToWindow\" "
           L"AutomaticResize=\"true\" DisplayWallpaper=\"false\" DisplayThemes=\"false\" EnableFontSmoothing=\"false\" "
           L"EnableDesktopComposition=\"false\" CacheBitmaps=\"false\" RedirectDiskDrives=\"false\" "
           L"RedirectPorts=\"false\" RedirectPrinters=\"false\" RedirectSmartCards=\"false\" RedirectSound=\"DoNotPlay\" "
           L"SoundQuality=\"Dynamic\" RedirectKeys=\"false\" Connected=\"false\" PreExtApp=\"\" PostExtApp=\"\" "
           L"MacAddress=\"\" UserField=\"\" ExtApp=\"\" VNCCompression=\"CompNone\" VNCEncoding=\"EncHextile\" "
           L"VNCAuthMode=\"AuthVNC\" VNCProxyType=\"ProxyNone\" VNCProxyIP=\"\" VNCProxyPort=\"0\" VNCProxyUsername=\"\" "
           L"VNCProxyPassword=\"\" VNCColors=\"ColNormal\" VNCSmartSizeMode=\"SmartSAspect\" VNCViewOnly=\"false\" "
           L"RDGatewayUsageMethod=\"Never\" RDGatewayHostname=\"\" RDGatewayUseConnectionCredentials=\"Yes\" "
           L"RDGatewayUsername=\"\" RDGatewayPassword=\"\" RDGatewayDomain=\"\"";
    tag << L" InheritCacheBitmaps=\"false\" InheritColors=\"false\" InheritDescription=\"false\" "
           L"InheritDisplayThemes=\"false\" InheritDisplayWallpaper=\"false\" InheritEnableFontSmoothing=\"false\" "
           L"InheritEnableDesktopComposition=\"false\" InheritDomain=\"false\" InheritIcon=\"false\" "
           L"InheritPanel=\"false\" InheritPassword=\"false\" InheritPort=\"false\" InheritProtocol=\"false\" "
           L"InheritPuttySession=\"false\" InheritRedirectDiskDrives=\"false\" InheritRedirectKeys=\"false\" "
           L"InheritRedirectPorts=\"false\" InheritRedirectPrinters=\"false\" InheritRedirectSmartCards=\"false\" "
           L"InheritRedirectSound=\"false\" InheritSoundQuality=\"false\" InheritResolution=\"false\" "
           L"InheritAutomaticResize=\"false\" InheritUseConsoleSession=\"false\" InheritUseCredSsp=\"false\" "
           L"InheritRenderingEngine=\"false\" InheritUsername=\"false\" InheritICAEncryptionStrength=\"false\" "
           L"InheritRDPAuthenticationLevel=\"false\" InheritRDPMinutesToIdleTimeout=\"false\" "
           L"InheritRDPAlertIdleTimeout=\"false\" InheritLoadBalanceInfo=\"false\" InheritPreExtApp=\"false\" "
           L"InheritPostExtApp=\"false\" InheritMacAddress=\"false\" InheritUserField=\"false\" InheritExtApp=\"false\" "
           L"InheritVNCCompression=\"false\" InheritVNCEncoding=\"false\" InheritVNCAuthMode=\"false\" "
           L"InheritVNCProxyType=\"false\" InheritVNCProxyIP=\"false\" InheritVNCProxyPort=\"false\" "
           L"InheritVNCProxyUsername=\"false\" InheritVNCProxyPassword=\"false\" InheritVNCColors=\"false\" "
           L"InheritVNCSmartSizeMode=\"false\" InheritVNCViewOnly=\"false\" InheritRDGatewayUsageMethod=\"false\" "
           L"InheritRDGatewayHostname=\"false\" InheritRDGatewayUseConnectionCredentials=\"false\" "
           L"InheritRDGatewayUsername=\"false\" InheritRDGatewayPassword=\"false\" InheritRDGatewayDomain=\"false\"";
    return tag.str();
}

bool ImportNode(IXMLDOMElement* element, ConnectionTreeModel& model, TreeNode& parent, MRemoteNgImportStats& stats,
                std::wstring& error) {
    const std::wstring type = GetAttr(element, L"Type");
    const std::wstring name = GetAttr(element, L"Name");
    if (name.empty()) {
        return true;
    }

    if (type == L"Container") {
        TreeNode* folder = model.AppendFolder(parent, name);
        if (!folder) {
            error = L"Failed to create folder.";
            return false;
        }
        ++stats.folders;

        IXMLDOMNodeList* children = nullptr;
        if (FAILED(element->get_childNodes(&children)) || !children) {
            return true;
        }
        long count = 0;
        children->get_length(&count);
        for (long i = 0; i < count; ++i) {
            IXMLDOMNode* childNode = nullptr;
            if (FAILED(children->get_item(i, &childNode)) || !childNode) {
                continue;
            }
            DOMNodeType nodeType = NODE_INVALID;
            childNode->get_nodeType(&nodeType);
            if (nodeType != NODE_ELEMENT) {
                childNode->Release();
                continue;
            }
            IXMLDOMElement* childElement = nullptr;
            if (SUCCEEDED(childNode->QueryInterface(IID_IXMLDOMElement, reinterpret_cast<void**>(&childElement))) &&
                childElement) {
                BSTR baseName = nullptr;
                childNode->get_baseName(&baseName);
                const bool isNode = baseName && wcscmp(baseName, L"Node") == 0;
                if (baseName) {
                    SysFreeString(baseName);
                }
                if (isNode && !ImportNode(childElement, model, *folder, stats, error)) {
                    childElement->Release();
                    childNode->Release();
                    children->Release();
                    return false;
                }
                childElement->Release();
            }
            childNode->Release();
        }
        children->Release();
        return true;
    }

    if (type != L"Connection") {
        return true;
    }

    const std::wstring protocol = GetAttr(element, L"Protocol");
    if (!protocol.empty() && protocol != L"RDP") {
        ++stats.skippedNonRdp;
        return true;
    }

    std::wstring host;
    int port = 3389;
    ParseHostPort(GetAttr(element, L"Hostname"), GetAttrInt(element, L"Port", 3389), host, port);
    if (host.empty()) {
        return true;
    }

    std::wstring username;
    std::wstring domain;
    ParseUsername(GetAttr(element, L"Username"), GetAttr(element, L"Domain"), username, domain);
    const std::wstring credentialId = FindOrCreateCredential(model, username, domain);

    TreeNode* session = model.AppendSession(parent, name, host, port, credentialId);
    if (!session) {
        error = L"Failed to create session.";
        return false;
    }
    ++stats.sessions;
    return true;
}

bool WriteUtf8File(const std::wstring& path, const std::string& utf8) {
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        return false;
    }
    out.write(utf8.data(), static_cast<std::streamsize>(utf8.size()));
    return static_cast<bool>(out);
}

void WriteExportNode(std::wostringstream& out, const TreeNode& node, const ConnectionTreeModel& model, int indent) {
    const std::wstring pad(indent, L' ');
    const std::wstring id = Util::NewUuid();

    std::wstring username;
    std::wstring domain;
    if (node.IsSession() && !node.credentialId.empty()) {
        if (const CredentialMeta* cred = model.FindCredential(node.credentialId)) {
            username = cred->username;
            domain = cred->domain;
        }
    }

    if (node.IsFolder()) {
        out << pad << BuildNodeOpenTag(node.name, L"Container", id, L"", L"", L"", L"", 3389, L"RDP") << L">\n";
        for (const auto& child : node.children) {
            WriteExportNode(out, *child, model, indent + 4);
        }
        out << pad << L"</Node>\n";
        return;
    }

    const std::wstring exportUser = FormatUsernameForExport(username, domain);
    out << pad << BuildNodeOpenTag(node.name, L"Connection", id, exportUser, L"", L"", node.host, node.port, L"RDP")
        << L" />\n";
}

void WriteExportNodes(std::wostringstream& out, const TreeNode& subtree, const ConnectionTreeModel& model) {
    if (subtree.IsFolder()) {
        for (const auto& child : subtree.children) {
            WriteExportNode(out, *child, model, 4);
        }
        return;
    }
    WriteExportNode(out, subtree, model, 4);
}

}  // namespace

bool MRemoteNgExchange::ImportFile(const std::wstring& path, ConnectionTreeModel& model, TreeNode& targetParent,
                                   MRemoteNgImportStats& stats, std::wstring& error) {
    stats = {};

    IXMLDOMDocument2* doc = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_DOMDocument60, nullptr, CLSCTX_INPROC_SERVER, IID_IXMLDOMDocument2,
                                  reinterpret_cast<void**>(&doc));
    if (FAILED(hr) || !doc) {
        error = L"Failed to create XML parser.";
        return false;
    }

    doc->put_async(VARIANT_FALSE);
    doc->put_validateOnParse(VARIANT_FALSE);
    doc->put_resolveExternals(VARIANT_FALSE);

    VARIANT fileVar{};
    VariantInit(&fileVar);
    fileVar.vt = VT_BSTR;
    fileVar.bstrVal = SysAllocString(path.c_str());

    VARIANT_BOOL loaded = VARIANT_FALSE;
    hr = doc->load(fileVar, &loaded);
    VariantClear(&fileVar);

    if (FAILED(hr) || loaded != VARIANT_TRUE) {
        doc->Release();
        error = L"Failed to read or parse XML file.";
        return false;
    }

    IXMLDOMElement* root = nullptr;
    if (FAILED(doc->get_documentElement(&root)) || !root) {
        doc->Release();
        error = L"XML file has no root element.";
        return false;
    }

    IXMLDOMNodeList* children = nullptr;
    if (FAILED(root->get_childNodes(&children)) || !children) {
        root->Release();
        doc->Release();
        error = L"No connection nodes found.";
        return false;
    }

    long count = 0;
    children->get_length(&count);
    bool ok = true;
    for (long i = 0; i < count && ok; ++i) {
        IXMLDOMNode* childNode = nullptr;
        if (FAILED(children->get_item(i, &childNode)) || !childNode) {
            continue;
        }
        DOMNodeType nodeType = NODE_INVALID;
        childNode->get_nodeType(&nodeType);
        if (nodeType != NODE_ELEMENT) {
            childNode->Release();
            continue;
        }
        IXMLDOMElement* childElement = nullptr;
        if (SUCCEEDED(childNode->QueryInterface(IID_IXMLDOMElement, reinterpret_cast<void**>(&childElement))) &&
            childElement) {
            BSTR baseName = nullptr;
            childNode->get_baseName(&baseName);
            const bool isNode = baseName && wcscmp(baseName, L"Node") == 0;
            if (baseName) {
                SysFreeString(baseName);
            }
            if (isNode) {
                ok = ImportNode(childElement, model, targetParent, stats, error);
            }
            childElement->Release();
        }
        childNode->Release();
    }

    children->Release();
    root->Release();
    doc->Release();
    return ok;
}

bool MRemoteNgExchange::ExportFile(const std::wstring& path, const TreeNode& subtree,
                                   const ConnectionTreeModel& model, std::wstring& error) {
    std::wostringstream body;
    body << L"<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
    body << L"<mrng:Connections xmlns:mrng=\"http://mremoteng.org\" Name=\"Connections\" Export=\"false\" "
            L"EncryptionEngine=\"AES\" BlockCipherMode=\"GCM\" KdfIterations=\"1000\" FullFileEncryption=\"false\" "
            L"Protected=\"\" ConfVersion=\"2.6\">\n";
    WriteExportNodes(body, subtree, model);
    body << L"</mrng:Connections>\n";

    const std::wstring wide = body.str();
    if (!WriteUtf8File(path, Util::WideToUtf8(wide))) {
        error = L"Failed to write export file.";
        return false;
    }
    return true;
}
