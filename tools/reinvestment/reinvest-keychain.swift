import Foundation
import Security
import LocalAuthentication

let service = "org.koinos.reinvest.wallet-password"
let args = CommandLine.arguments
func fail(_ message: String) -> Never {
    FileHandle.standardError.write(Data((message + "\n").utf8))
    exit(1)
}
guard args.count >= 3 else { fail("Expected command and producer account") }
let account = args[2]
let query: [String: Any] = [
    kSecClass as String: kSecClassGenericPassword,
    kSecAttrService as String: service,
    kSecAttrAccount as String: account
]
guard args.count >= 2 else { fail("Expected import, read or check") }
if args[1] == "import" {
    guard args.count == 4 else { fail("Expected source password file path") }
    let url = URL(fileURLWithPath: args[3])
    do {
        let attributes = try FileManager.default.attributesOfItem(atPath: url.path)
        guard attributes[.type] as? FileAttributeType == .typeRegular,
              let permissions = attributes[.posixPermissions] as? NSNumber,
              permissions.intValue & 0o077 == 0,
              (attributes[.ownerAccountID] as? NSNumber)?.uint32Value == getuid()
        else { fail("Source must be an owned private regular file") }
        var password = try Data(contentsOf: url)
        defer { password.resetBytes(in: 0..<password.count) }
        if password.last == 10 { password.removeLast(); if password.last == 13 { password.removeLast() } }
        guard !password.isEmpty else { fail("Empty password") }
        var item = query
        item[kSecValueData as String] = password
        item[kSecAttrLabel as String] = "Koinos Teleno reward reinvestment"
        let status = SecItemAdd(item as CFDictionary, nil)
        if status == errSecDuplicateItem { fail("Item already exists; not overwritten") }
        guard status == errSecSuccess else { fail("Keychain import failed: \(status)") }
        print("Password stored in Keychain; source file preserved")
    } catch { fail("Cannot import protected password file") }
} else if args[1] == "read" || args[1] == "check" {
    var request = query
    request[kSecReturnData as String] = true
    request[kSecMatchLimit as String] = kSecMatchLimitOne
    // Unattended jobs must fail instead of hanging on an authorization dialog.
    let context = LAContext()
    context.interactionNotAllowed = true
    request[kSecUseAuthenticationContext as String] = context
    var result: CFTypeRef?
    let status = SecItemCopyMatching(request as CFDictionary, &result)
    guard status == errSecSuccess, var password = result as? Data, !password.isEmpty
    else { fail("Keychain read unavailable or access denied: \(status)") }
    defer { password.resetBytes(in: 0..<password.count) }
    if args[1] == "read" { FileHandle.standardOutput.write(password) }
    else { print("Keychain credential accessible; no secret displayed") }
} else { fail("Unknown command") }
