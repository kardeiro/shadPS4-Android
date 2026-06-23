package com.shadps4.emulator.ui.screens.library

import android.net.Uri
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.statusBars
import androidx.compose.foundation.layout.windowInsetsTopHeight
import androidx.compose.foundation.lazy.grid.GridCells
import androidx.compose.foundation.lazy.grid.LazyVerticalGrid
import androidx.compose.foundation.lazy.grid.items
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.Add
import androidx.compose.material.icons.rounded.FolderOpen
import androidx.compose.material.icons.rounded.Search
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.ExtendedFloatingActionButton
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Snackbar
import androidx.compose.material3.SnackbarHost
import androidx.compose.material3.SnackbarHostState
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.lifecycle.viewmodel.compose.viewModel
import com.shadps4.emulator.R
import com.shadps4.emulator.data.model.GameInfo
import com.shadps4.emulator.data.model.SampleGames
import com.shadps4.emulator.data.native.ShadPs4Native
import com.shadps4.emulator.ui.components.GameCard
import com.shadps4.emulator.viewmodel.LibraryViewModel
import kotlinx.coroutines.launch

@Composable
fun LibraryScreen(
    onGameClick: (String) -> Unit,
    onAddFolder: () -> Unit,
    onInstallPkg: () -> Unit,
) {
    val viewModel: LibraryViewModel = viewModel()
    val context = LocalContext.current
    val games by viewModel.games.collectAsState()
    val importState by viewModel.importState.collectAsState()
    val snackbarHostState = remember { SnackbarHostState() }
    val scope = rememberCoroutineScope()

    var query by remember { mutableStateOf("") }
    val filteredGames = remember(query, games) {
        if (query.isBlank()) games
        else games.filter {
            it.title.contains(query, ignoreCase = true) ||
                it.serial.contains(query, ignoreCase = true)
        }
    }

    // SAF file picker: launches the system file picker for any "*.sfo" file.
    // Phase 2 will accept "*.pkg" here and route through the PKG installer.
    val pickSfo = rememberLauncherForActivityResult(
        contract = ActivityResultContracts.OpenDocument()
    ) { uri: Uri? ->
        if (uri != null) {
            // Persist permission so we can re-read the file later
            context.contentResolver.takePersistableUriPermission(
                uri,
                android.content.Intent.FLAG_GRANT_READ_URI_PERMISSION,
            )
            viewModel.importParamSfo(uri)
        }
    }

    // Snackbar feedback for the import flow.
    LaunchedEffect(importState) {
        when (val s = importState) {
            is LibraryViewModel.ImportState.Success -> {
                scope.launch {
                    snackbarHostState.showSnackbar(
                        "Imported: ${s.game.title} (${s.game.serial})"
                    )
                    viewModel.resetImportState()
                }
            }
            is LibraryViewModel.ImportState.Error -> {
                scope.launch {
                    snackbarHostState.showSnackbar(s.message)
                    viewModel.resetImportState()
                }
            }
            else -> Unit
        }
    }

    Box(modifier = Modifier.fillMaxSize()) {
        Column(modifier = Modifier.fillMaxSize()) {
            Spacer(Modifier.windowInsetsTopHeight(WindowInsets.statusBars))

            // Header
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(horizontal = 16.dp, vertical = 12.dp),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Text(
                    text = stringResource(R.string.library_title),
                    style = MaterialTheme.typography.headlineMedium,
                    fontWeight = FontWeight.Bold,
                    modifier = Modifier.weight(1f),
                )
                ExtendedFloatingActionButton(
                    onClick = {
                        pickSfo.launch(arrayOf("*/*"))
                    },
                    icon = {
                        if (importState is LibraryViewModel.ImportState.Loading) {
                            CircularProgressIndicator(
                                modifier = Modifier.size(20.dp),
                                strokeWidth = 2.dp,
                            )
                        } else {
                            Icon(Icons.Rounded.Add, contentDescription = null)
                        }
                    },
                    text = { Text(stringResource(R.string.library_install_pkg)) },
                    containerColor = MaterialTheme.colorScheme.primaryContainer,
                    contentColor = MaterialTheme.colorScheme.onPrimaryContainer,
                )
            }

            // Search bar
            OutlinedTextField(
                value = query,
                onValueChange = { query = it },
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(horizontal = 16.dp),
                placeholder = { Text(stringResource(R.string.search)) },
                leadingIcon = { Icon(Icons.Rounded.Search, contentDescription = null) },
                singleLine = true,
                shape = MaterialTheme.shapes.large,
            )

            // Native module status badge (tiny, below search)
            NativeStatusBadge()

            // Grid
            if (filteredGames.isEmpty()) {
                EmptyLibrary()
            } else {
                LazyVerticalGrid(
                    columns = GridCells.Adaptive(minSize = 150.dp),
                    contentPadding = PaddingValues(16.dp),
                    verticalArrangement = Arrangement.spacedBy(12.dp),
                    horizontalArrangement = Arrangement.spacedBy(12.dp),
                ) {
                    items(filteredGames, key = { it.id }) { game ->
                        GameCard(
                            game = game,
                            onClick = { onGameClick(game.id) },
                        )
                    }
                }
            }
        }

        SnackbarHost(
            hostState = snackbarHostState,
            modifier = Modifier.align(Alignment.BottomCenter),
        ) { data ->
            Snackbar(snackbarData = data)
        }
    }
}

@Composable
private fun NativeStatusBadge() {
    val loaded = remember { ShadPs4Native.isLoaded }
    val version = remember { if (loaded) ShadPs4Native.nativeVersion() else "native: not loaded" }
    Text(
        text = version,
        style = MaterialTheme.typography.labelSmall,
        color = if (loaded) MaterialTheme.colorScheme.onSurfaceVariant
                else MaterialTheme.colorScheme.error,
        modifier = Modifier.padding(horizontal = 16.dp, vertical = 4.dp),
    )
}

@Composable
private fun EmptyLibrary() {
    Box(
        modifier = Modifier
            .fillMaxSize()
            .padding(32.dp),
        contentAlignment = Alignment.Center,
    ) {
        Column(
            horizontalAlignment = Alignment.CenterHorizontally,
            verticalArrangement = Arrangement.spacedBy(8.dp),
        ) {
            Icon(
                imageVector = Icons.Rounded.FolderOpen,
                contentDescription = null,
                modifier = Modifier.size(64.dp),
                tint = MaterialTheme.colorScheme.onSurfaceVariant,
            )
            Text(
                text = stringResource(R.string.library_empty),
                style = MaterialTheme.typography.bodyMedium,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
        }
    }
}
