package app.cosmic.browser.mobile.vault

import java.nio.file.Files
import java.util.concurrent.CountDownLatch
import java.util.concurrent.atomic.AtomicReference
import org.junit.Test

class PasswordVaultTest {
    @Test
    fun vaultRoundtrip() {
        val directory = Files.createTempDirectory("cosmic-vault-test")
        try {
            val file = directory.resolve("vault.dat").toFile()
            val master = "a long unique test passphrase".toCharArray()
            val wrong = "another test passphrase".toCharArray()
            val first = PasswordVault(file)
            expect<VaultException.Locked> { first.list() }
            expect<VaultException.InvalidInput> { first.initialize("short".toCharArray()) }
            first.initialize(master)
            expect<VaultException.AlreadyExists> { PasswordVault(file).initialize(master) }
            expect<VaultException.InvalidInput> { first.create("Sample", "", "\uD800", "", "") }
            val created = first.create("Sample", "test-user", "secret-for-test", "https://example.test", "notes")
            check(first.get(created.id).password == "secret-for-test")
            check(!file.readBytes().toString(Charsets.ISO_8859_1).contains("secret-for-test"))
            first.update(VaultEntry(created.id, "Changed", "user", "new-secret", "", ""))
            check(first.list().single().title == "Changed")
            expect<VaultException.NotFound> { first.delete("missing") }
            first.lock()
            check(!first.isUnlocked)
            expect<VaultException.Locked> { first.list() }
            expect<VaultException.AuthenticationFailed> { first.unlock(wrong) }
            check(!first.isUnlocked)
            first.unlock(master)
            check(first.get(created.id).password == "new-secret")

            val second = PasswordVault(file)
            second.unlock(master)
            val start = CountDownLatch(1)
            val failure = AtomicReference<Throwable>()
            val threads = listOf(first, second).mapIndexed { index, vault ->
                Thread {
                    try {
                        start.await()
                        vault.create("Concurrent $index", "", "", "", "")
                    } catch (e: Throwable) {
                        failure.set(e)
                    }
                }.also { it.start() }
            }
            start.countDown()
            threads.forEach { it.join() }
            failure.get()?.let { throw it }
            check(first.list().size == 3)

            val original = file.readBytes()
            val tampered = original.clone().also { it[it.lastIndex] = (it.last().toInt() xor 1).toByte() }
            file.writeBytes(tampered)
            expect<VaultException.AuthenticationFailed> { first.create("Must not write", "", "", "", "") }
            check(file.readBytes().contentEquals(tampered))
            second.lock()
            expect<VaultException.AuthenticationFailed> { second.unlock(master) }
            val modifiedHeader = original.clone().also { it[32] = (it[32].toInt() xor 1).toByte() }
            file.writeBytes(modifiedHeader)
            expect<VaultException.AuthenticationFailed> { second.unlock(master) }
            file.writeBytes(original)
            first.delete(created.id)
            check(first.list().size == 2)
            first.lock()
            second.unlock(master)
            check(second.list().size == 2)
            second.lock()

            file.writeBytes(original.copyOf(12))
            expect<VaultException.InvalidFormat> { first.unlock(master) }
            master.fill('\u0000')
            wrong.fill('\u0000')
            println("PasswordVault JVM smoke tests passed")
        } finally {
            Files.walk(directory).use { paths ->
                paths.sorted(Comparator.reverseOrder()).forEach { Files.deleteIfExists(it) }
            }
        }
    }

    private inline fun <reified T : Throwable> expect(block: () -> Unit) {
        try {
            block()
        } catch (e: Throwable) {
            if (e is T) return
            throw e
        }
        error("Expected ${T::class.java.simpleName}")
    }
}
