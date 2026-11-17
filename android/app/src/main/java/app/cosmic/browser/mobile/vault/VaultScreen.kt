package app.cosmic.browser.mobile.vault

import android.net.Uri
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.text.selection.SelectionContainer
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.unit.dp
import java.io.File
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

@Composable
fun VaultScreen(onBack: () -> Unit) {
    val context = LocalContext.current
    val file = remember { File(context.noBackupFilesDir, "password-vault.dat") }
    val vault = remember { PasswordVault(file) }
    val scope = rememberCoroutineScope()
    var initialized by remember { mutableStateOf(file.exists()) }
    var unlocked by remember { mutableStateOf(false) }
    var working by remember { mutableStateOf(false) }
    var error by remember { mutableStateOf<String?>(null) }
    var master by remember { mutableStateOf("") }
    var confirmation by remember { mutableStateOf("") }
    var entries by remember { mutableStateOf(emptyList<VaultEntry>()) }
    var selected by remember { mutableStateOf<VaultEntry?>(null) }
    var adding by remember { mutableStateOf(false) }
    var deleting by remember { mutableStateOf<VaultEntry?>(null) }

    DisposableEffect(vault) {
        onDispose { vault.lock() }
    }

    fun execute(block: () -> List<VaultEntry>) {
        if (working) return
        working = true
        error = null
        scope.launch {
            try {
                entries = withContext(Dispatchers.IO) { block() }
                initialized = file.exists()
                unlocked = vault.isUnlocked
            } catch (failure: VaultException) {
                error = failure.message ?: "Operazione non riuscita"
            } finally {
                working = false
            }
        }
    }

    Column(Modifier.fillMaxSize().padding(16.dp), verticalArrangement = Arrangement.spacedBy(12.dp)) {
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            TextButton(onClick = onBack) { Text("Indietro") }
            Text("Password Vault")
        }
        if (!unlocked) {
            Text(if (initialized) "Sblocca il vault" else "Crea un vault locale cifrato")
            Text("Usa una frase segreta unica di almeno 16 caratteri.")
            OutlinedTextField(
                value = master,
                onValueChange = { master = it },
                label = { Text("Password principale") },
                visualTransformation = PasswordVisualTransformation(),
                singleLine = true,
                enabled = !working,
                modifier = Modifier.fillMaxWidth()
            )
            if (!initialized) {
                OutlinedTextField(
                    value = confirmation,
                    onValueChange = { confirmation = it },
                    label = { Text("Conferma password principale") },
                    visualTransformation = PasswordVisualTransformation(),
                    singleLine = true,
                    enabled = !working,
                    modifier = Modifier.fillMaxWidth()
                )
            }
            Button(enabled = !working && master.isNotEmpty(), onClick = {
                if (!initialized && master != confirmation) {
                    error = "Le password non coincidono."
                } else {
                    val password = master.toCharArray()
                    master = ""
                    confirmation = ""
                    execute {
                        try {
                            if (initialized) vault.unlock(password) else vault.initialize(password)
                            vault.list()
                        } finally {
                            password.fill('\u0000')
                        }
                    }
                }
            }) { Text(if (initialized) "Sblocca" else "Crea vault") }
        } else {
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                Button(enabled = !working, onClick = { adding = true }) { Text("Aggiungi") }
                TextButton(enabled = !working, onClick = {
                    vault.lock()
                    entries = emptyList()
                    selected = null
                    unlocked = false
                }) { Text("Blocca") }
            }
            if (entries.isEmpty()) Text("Nessuna credenziale salvata.")
            LazyColumn(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                items(entries, key = { it.id }) { entry ->
                    Card(Modifier.fillMaxWidth()) {
                        Column(Modifier.padding(12.dp)) {
                            Text(entry.title)
                            if (entry.username.isNotEmpty()) Text(entry.username)
                            Row {
                                TextButton(onClick = { selected = entry }) { Text("Mostra") }
                                TextButton(onClick = { deleting = entry }) { Text("Elimina") }
                            }
                        }
                    }
                }
            }
        }
        if (working) Text("Operazione in corso…")
        error?.let { Text(it) }
    }

    if (adding) {
        var site by remember { mutableStateOf("") }
        var username by remember { mutableStateOf("") }
        var password by remember { mutableStateOf("") }
        var validation by remember { mutableStateOf<String?>(null) }
        AlertDialog(
            onDismissRequest = { adding = false },
            title = { Text("Nuova credenziale") },
            text = {
                Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                    OutlinedTextField(site, { site = it }, label = { Text("Indirizzo HTTPS") }, singleLine = true)
                    OutlinedTextField(username, { username = it }, label = { Text("Nome utente") }, singleLine = true)
                    OutlinedTextField(
                        password, { password = it }, label = { Text("Password") },
                        visualTransformation = PasswordVisualTransformation(), singleLine = true
                    )
                    validation?.let { Text(it) }
                }
            },
            confirmButton = {
                TextButton(onClick = {
                    val url = Uri.parse(site.trim())
                    val host = url.host
                    if (url.scheme != "https" || host.isNullOrBlank() || password.isEmpty()) {
                        validation = "Inserisci un indirizzo HTTPS valido e una password."
                    } else {
                        val savedUrl = site.trim()
                        val savedUsername = username
                        val savedPassword = password
                        password = ""
                        adding = false
                        execute {
                            vault.create(host, savedUsername, savedPassword, savedUrl, "")
                            vault.list()
                        }
                    }
                }) { Text("Salva") }
            },
            dismissButton = { TextButton(onClick = { adding = false }) { Text("Annulla") } }
        )
    }

    selected?.let { entry ->
        AlertDialog(
            onDismissRequest = { selected = null },
            title = { Text(entry.title) },
            text = {
                SelectionContainer {
                    Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                        Text(entry.url)
                        Text(entry.username)
                        Text(entry.password)
                    }
                }
            },
            confirmButton = { TextButton(onClick = { selected = null }) { Text("Chiudi") } }
        )
    }
    deleting?.let { entry ->
        AlertDialog(
            onDismissRequest = { deleting = null },
            title = { Text("Eliminare la credenziale?") },
            text = { Text(entry.title) },
            confirmButton = {
                TextButton(onClick = {
                    deleting = null
                    execute {
                        vault.delete(entry.id)
                        vault.list()
                    }
                }) { Text("Elimina") }
            },
            dismissButton = { TextButton(onClick = { deleting = null }) { Text("Annulla") } }
        )
    }
}
