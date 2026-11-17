package app.cosmic.browser.mobile.vault

import java.io.ByteArrayInputStream
import java.io.ByteArrayOutputStream
import java.io.DataInputStream
import java.io.DataOutputStream
import java.io.File
import java.io.FileOutputStream
import java.io.IOException
import java.nio.ByteBuffer
import java.nio.CharBuffer
import java.nio.channels.FileChannel
import java.nio.charset.CharacterCodingException
import java.nio.charset.CodingErrorAction
import java.nio.charset.StandardCharsets
import java.nio.file.Files
import java.nio.file.StandardCopyOption
import java.nio.file.StandardOpenOption
import java.security.GeneralSecurityException
import java.security.SecureRandom
import java.util.UUID
import javax.crypto.BadPaddingException
import javax.crypto.Cipher
import javax.crypto.SecretKeyFactory
import javax.crypto.spec.GCMParameterSpec
import javax.crypto.spec.PBEKeySpec
import javax.crypto.spec.SecretKeySpec

/** An explicit, deliberately non-printable credential. Callers must protect their own copies. */
class VaultEntry(
    val id: String,
    val title: String,
    val username: String,
    val password: String,
    val url: String,
    val notes: String
)

/** Messages contain no credential values. An authentication failure can also mean corruption. */
sealed class VaultException(message: String, cause: Throwable? = null) : Exception(message, cause) {
    class Locked : VaultException("Vault is locked")
    class AlreadyExists : VaultException("Vault already exists")
    class NotFound : VaultException("Vault or entry not found")
    class InvalidInput : VaultException("Invalid input")
    class AuthenticationFailed : VaultException("Incorrect master password or damaged vault")
    class InvalidFormat : VaultException("Unsupported or damaged vault")
    class Storage(cause: Throwable) : VaultException("Vault storage error", cause)
    class Crypto(cause: Throwable) : VaultException("Vault cryptography unavailable", cause)
}

/**
 * Synchronous API: perform every operation on a background dispatcher, not the UI thread.
 *
 * Supply a file in Context.noBackupFilesDir, e.g. File(context.noBackupFilesDir, "vault.dat").
 * All instances/processes using this file must use this implementation and the same path. Do not
 * delete the adjacent lock file while the vault is in use. No plaintext is intentionally written.
 * The master password belongs to the caller; clear its CharArray after use. lock() clears the
 * retained derived key on a best-effort basis, not all JVM/provider/UI copies.
 * The file is synced before atomic replacement, but directory metadata is not synced; after sudden
 * power loss the latest acknowledged change may be lost. There is no rollback protection.
 */
class PasswordVault(file: File) {
    private val file = try { file.canonicalFile } catch (e: IOException) { throw VaultException.Storage(e) }
    private val lockFile = File(this.file.path + ".lock")
    private val processLock = locks.computeIfAbsent(this.file.path) { Any() }
    private val random = SecureRandom()
    private var key: ByteArray? = null
    private var salt: ByteArray? = null
    private var iterations: Int = 0

    val isUnlocked: Boolean
        get() = synchronized(processLock) { key != null }

    /** A minimum length is enforced, but callers should guide users to a unique random passphrase. */
    fun initialize(masterPassword: CharArray) = guarded {
        validatePassword(masterPassword)
        if (file.exists()) throw VaultException.AlreadyExists()
        val newSalt = ByteArray(SALT_SIZE).also(random::nextBytes)
        val newKey = derive(masterPassword, newSalt, KDF_ITERATIONS)
        try {
            writeVault(newKey, newSalt, KDF_ITERATIONS, emptyList())
            clearKey()
            key = newKey
            salt = newSalt
            iterations = KDF_ITERATIONS
        } catch (e: Exception) {
            newKey.fill(0)
            throw e
        }
    }

    /** Failed unlock leaves this instance locked. Do not reject older passwords by new policy. */
    fun unlock(masterPassword: CharArray) = guarded {
        clearKey()
        if (masterPassword.isEmpty() || masterPassword.size > MAX_MASTER_CHARS) {
            throw VaultException.InvalidInput()
        }
        val blob = readBlob()
        val header = parseHeader(blob)
        val newKey = derive(masterPassword, header.salt, header.iterations)
        try {
            decrypt(blob, header, newKey)
            key = newKey
            salt = header.salt
            iterations = header.iterations
        } catch (e: Exception) {
            newKey.fill(0)
            throw e
        }
    }

    fun lock() = synchronized(processLock) { clearKey() }

    fun list(): List<VaultEntry> = guarded { readUnlocked() }

    fun get(id: String): VaultEntry = guarded {
        readUnlocked().firstOrNull { it.id == id } ?: throw VaultException.NotFound()
    }

    /** Creates a new entry with a random ID. */
    fun create(title: String, username: String, password: String, url: String, notes: String): VaultEntry =
        guarded {
            val entries = readUnlocked().toMutableList()
            val entry = VaultEntry(UUID.randomUUID().toString(), title, username, password, url, notes)
            validateEntry(entry)
            entries.add(entry)
            writeUnlocked(entries)
            entry
        }

    /** Replaces an existing entry; its ID cannot be changed. */
    fun update(entry: VaultEntry) = guarded {
        validateEntry(entry)
        val entries = readUnlocked().toMutableList()
        val index = entries.indexOfFirst { it.id == entry.id }
        if (index < 0) throw VaultException.NotFound()
        entries[index] = entry
        writeUnlocked(entries)
    }

    fun delete(id: String) = guarded {
        val entries = readUnlocked().toMutableList()
        if (!entries.removeAll { it.id == id }) throw VaultException.NotFound()
        writeUnlocked(entries)
    }

    private fun clearKey() {
        key?.fill(0)
        key = null
        salt = null
        iterations = 0
    }

    private fun readUnlocked(): List<VaultEntry> {
        val currentKey = key ?: throw VaultException.Locked()
        val blob = readBlob()
        val header = parseHeader(blob)
        if (iterations != header.iterations || !header.salt.contentEquals(salt)) {
            clearKey()
            throw VaultException.AuthenticationFailed()
        }
        return decrypt(blob, header, currentKey)
    }

    private fun writeUnlocked(entries: List<VaultEntry>) {
        writeVault(key ?: throw VaultException.Locked(), salt ?: throw VaultException.Locked(), iterations, entries)
    }

    private fun validatePassword(password: CharArray) {
        if (password.size > MAX_MASTER_CHARS ||
            Character.codePointCount(password, 0, password.size) < MIN_MASTER_CODEPOINTS ||
            password.all { it.isWhitespace() }
        ) throw VaultException.InvalidInput()
    }

    private fun validateEntry(entry: VaultEntry) {
        if (entry.id.length != 36 || try { UUID.fromString(entry.id).toString() != entry.id } catch (_: IllegalArgumentException) { true }) {
            throw VaultException.InvalidInput()
        }
        if (entry.title.isBlank()) throw VaultException.InvalidInput()
        for (value in listOf(entry.title, entry.username, entry.password, entry.url, entry.notes)) {
            if (value.length > MAX_FIELD_BYTES) throw VaultException.InvalidInput()
            val encoded = try {
                StandardCharsets.UTF_8.newEncoder()
                    .onMalformedInput(CodingErrorAction.REPORT)
                    .onUnmappableCharacter(CodingErrorAction.REPORT)
                    .encode(CharBuffer.wrap(value))
            } catch (_: CharacterCodingException) {
                throw VaultException.InvalidInput()
            }
            val size = encoded.remaining()
            if (encoded.hasArray()) encoded.array().fill(0)
            if (size > MAX_FIELD_BYTES) throw VaultException.InvalidInput()
        }
    }

    private fun derive(password: CharArray, salt: ByteArray, iterations: Int): ByteArray {
        val spec = PBEKeySpec(password, salt, iterations, 256)
        return try {
            SecretKeyFactory.getInstance("PBKDF2WithHmacSHA256").generateSecret(spec).encoded
        } finally {
            spec.clearPassword()
        }
    }

    private data class Header(val bytes: ByteArray, val salt: ByteArray, val nonce: ByteArray, val iterations: Int)

    private fun parseHeader(blob: ByteArray): Header {
        if (blob.size < HEADER_SIZE + TAG_SIZE || blob.size > MAX_BLOB_BYTES) throw VaultException.InvalidFormat()
        val bytes = blob.copyOfRange(0, HEADER_SIZE)
        val buffer = ByteBuffer.wrap(bytes)
        val magic = ByteArray(MAGIC.size).also(buffer::get)
        if (!magic.contentEquals(MAGIC) || buffer.int != VERSION) throw VaultException.InvalidFormat()
        val iterations = buffer.int
        if (iterations !in MIN_KDF_ITERATIONS..MAX_KDF_ITERATIONS) throw VaultException.InvalidFormat()
        val salt = ByteArray(SALT_SIZE).also(buffer::get)
        val nonce = ByteArray(NONCE_SIZE).also(buffer::get)
        return Header(bytes, salt, nonce, iterations)
    }

    private fun readBlob(): ByteArray {
        if (!file.exists()) throw VaultException.NotFound()
        // A bounded read avoids allocating based on an attacker-controlled file length.
        file.inputStream().use { input ->
            val out = ByteArrayOutputStream()
            val buffer = ByteArray(8192)
            while (true) {
                val count = input.read(buffer)
                if (count < 0) break
                if (out.size() + count > MAX_BLOB_BYTES) throw VaultException.InvalidFormat()
                out.write(buffer, 0, count)
            }
            return out.toByteArray()
        }
    }

    private fun decrypt(blob: ByteArray, header: Header, key: ByteArray): List<VaultEntry> {
        val cipher = Cipher.getInstance("AES/GCM/NoPadding")
        cipher.init(Cipher.DECRYPT_MODE, SecretKeySpec(key, "AES"), GCMParameterSpec(128, header.nonce))
        cipher.updateAAD(header.bytes)
        val plaintext = try {
            cipher.doFinal(blob, HEADER_SIZE, blob.size - HEADER_SIZE)
        } catch (_: BadPaddingException) {
            throw VaultException.AuthenticationFailed()
        }
        return try {
            decode(plaintext)
        } finally {
            plaintext.fill(0)
        }
    }

    private fun encode(entries: List<VaultEntry>): ByteArray {
        if (entries.size > MAX_ENTRIES) throw VaultException.InvalidInput()
        val out = ByteArrayOutputStream()
        DataOutputStream(out).use { data ->
            data.writeInt(entries.size)
            for (entry in entries) {
                validateEntry(entry)
                for (value in listOf(entry.id, entry.title, entry.username, entry.password, entry.url, entry.notes)) {
                    val bytes = value.toByteArray(StandardCharsets.UTF_8)
                    if (out.size() + 4L + bytes.size > MAX_PLAINTEXT_BYTES) throw VaultException.InvalidInput()
                    data.writeInt(bytes.size)
                    data.write(bytes)
                    bytes.fill(0)
                }
            }
        }
        return out.toByteArray()
    }

    private fun decode(bytes: ByteArray): List<VaultEntry> {
        try {
            val input = DataInputStream(ByteArrayInputStream(bytes))
            val count = input.readInt()
            if (count !in 0..MAX_ENTRIES) throw VaultException.InvalidFormat()
            val entries = ArrayList<VaultEntry>(count)
            val ids = HashSet<String>()
            repeat(count) {
                fun field(): String {
                    val size = input.readInt()
                    if (size < 0 || size > MAX_FIELD_BYTES || size > input.available()) throw VaultException.InvalidFormat()
                    val raw = ByteArray(size)
                    input.readFully(raw)
                    return try {
                        StandardCharsets.UTF_8.newDecoder()
                            .onMalformedInput(CodingErrorAction.REPORT)
                            .onUnmappableCharacter(CodingErrorAction.REPORT)
                            .decode(ByteBuffer.wrap(raw)).toString()
                    } finally {
                        raw.fill(0)
                    }
                }
                val entry = VaultEntry(field(), field(), field(), field(), field(), field())
                try { validateEntry(entry) } catch (_: VaultException.InvalidInput) { throw VaultException.InvalidFormat() }
                if (!ids.add(entry.id)) throw VaultException.InvalidFormat()
                entries.add(entry)
            }
            if (input.available() != 0) throw VaultException.InvalidFormat()
            return entries
        } catch (_: IOException) {
            throw VaultException.InvalidFormat()
        }
    }

    private fun writeVault(key: ByteArray, salt: ByteArray, iterations: Int, entries: List<VaultEntry>) {
        val nonce = ByteArray(NONCE_SIZE).also(random::nextBytes)
        val header = ByteBuffer.allocate(HEADER_SIZE).put(MAGIC).putInt(VERSION).putInt(iterations)
            .put(salt).put(nonce).array()
        val plaintext = encode(entries)
        val encrypted = try {
            val cipher = Cipher.getInstance("AES/GCM/NoPadding")
            cipher.init(Cipher.ENCRYPT_MODE, SecretKeySpec(key, "AES"), GCMParameterSpec(128, nonce))
            cipher.updateAAD(header)
            cipher.doFinal(plaintext)
        } finally {
            plaintext.fill(0)
        }
        // The temporary file contains only authenticated ciphertext and is on the same filesystem.
        val temp = Files.createTempFile(file.parentFile.toPath(), ".vault-", ".tmp")
        try {
            FileOutputStream(temp.toFile()).use { output ->
                output.write(header)
                output.write(encrypted)
                output.fd.sync()
            }
            Files.move(temp, file.toPath(), StandardCopyOption.ATOMIC_MOVE, StandardCopyOption.REPLACE_EXISTING)
        } finally {
            Files.deleteIfExists(temp)
        }
    }

    private fun <T> guarded(block: () -> T): T = synchronized(processLock) {
        try {
            FileChannel.open(lockFile.toPath(), StandardOpenOption.CREATE, StandardOpenOption.WRITE).use { channel ->
                channel.lock().use { block() }
            }
        } catch (e: VaultException) {
            throw e
        } catch (e: IOException) {
            throw VaultException.Storage(e)
        } catch (e: GeneralSecurityException) {
            throw VaultException.Crypto(e)
        }
    }

    companion object {
        private val locks = java.util.concurrent.ConcurrentHashMap<String, Any>()
        private val MAGIC = byteArrayOf(0x43, 0x4f, 0x53, 0x56, 0x4c, 0x54, 0x30, 0x31)
        private const val VERSION = 1
        private const val KDF_ITERATIONS = 600_000
        private const val MIN_KDF_ITERATIONS = 600_000
        private const val MAX_KDF_ITERATIONS = 2_000_000
        private const val SALT_SIZE = 16
        private const val NONCE_SIZE = 12
        private const val TAG_SIZE = 16
        private const val HEADER_SIZE = 8 + 4 + 4 + SALT_SIZE + NONCE_SIZE
        private const val MIN_MASTER_CODEPOINTS = 16
        private const val MAX_MASTER_CHARS = 1024
        private const val MAX_FIELD_BYTES = 64 * 1024
        private const val MAX_ENTRIES = 1000
        private const val MAX_PLAINTEXT_BYTES = 4 * 1024 * 1024
        private const val MAX_BLOB_BYTES = HEADER_SIZE + TAG_SIZE + MAX_PLAINTEXT_BYTES
    }
}
