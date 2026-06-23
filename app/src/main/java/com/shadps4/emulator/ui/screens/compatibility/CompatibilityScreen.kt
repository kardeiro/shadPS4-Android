package com.shadps4.emulator.ui.screens.compatibility

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.statusBars
import androidx.compose.foundation.layout.windowInsetsTopHeight
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.Search
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.FilterChip
import androidx.compose.material3.FilterChipDefaults
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import com.shadps4.emulator.R
import com.shadps4.emulator.data.model.CompatibilityStatus
import com.shadps4.emulator.data.model.SampleGames
import com.shadps4.emulator.ui.components.CompatibilityBadge

@Composable
fun CompatibilityScreen(
    onGameClick: (String) -> Unit,
) {
    var query by remember { mutableStateOf("") }
    var filter by remember { mutableStateOf(CompatibilityStatus.UNKNOWN) }

    val allGames = remember { SampleGames.all }
    val games = remember(query, filter) {
        allGames.filter { game ->
            val matchesQuery = query.isBlank() ||
                game.title.contains(query, ignoreCase = true) ||
                game.serial.contains(query, ignoreCase = true)
            val matchesFilter = filter == CompatibilityStatus.UNKNOWN || game.compatibility == filter
            matchesQuery && matchesFilter
        }
    }

    Column(modifier = Modifier.fillMaxSize()) {
        Spacer(Modifier.windowInsetsTopHeight(WindowInsets.statusBars))

        Text(
            text = stringResource(R.string.compatibility_title),
            style = MaterialTheme.typography.headlineMedium,
            fontWeight = FontWeight.Bold,
            modifier = Modifier.padding(horizontal = 16.dp, vertical = 12.dp),
        )

        OutlinedTextField(
            value = query,
            onValueChange = { query = it },
            modifier = Modifier
                .fillMaxWidth()
                .padding(horizontal = 16.dp),
            placeholder = { Text(stringResource(R.string.compatibility_search_hint)) },
            leadingIcon = { Icon(Icons.Rounded.Search, contentDescription = null) },
            singleLine = true,
            shape = MaterialTheme.shapes.large,
        )

        // Filter chips
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .padding(horizontal = 16.dp, vertical = 8.dp),
            horizontalArrangement = Arrangement.spacedBy(8.dp),
        ) {
            FilterChip(
                selected = filter == CompatibilityStatus.UNKNOWN,
                onClick = { filter = CompatibilityStatus.UNKNOWN },
                label = { Text(stringResource(R.string.compatibility_filter_all)) },
            )
            FilterChip(
                selected = filter == CompatibilityStatus.PLAYABLE,
                onClick = { filter = CompatibilityStatus.PLAYABLE },
                label = { Text(stringResource(R.string.compatibility_filter_playable)) },
                colors = FilterChipDefaults.filterChipColors(
                    selectedContainerColor = Color(0xFF1B873F).copy(alpha = 0.2f),
                ),
            )
            FilterChip(
                selected = filter == CompatibilityStatus.INGAME,
                onClick = { filter = CompatibilityStatus.INGAME },
                label = { Text(stringResource(R.string.compatibility_filter_ingame)) },
                colors = FilterChipDefaults.filterChipColors(
                    selectedContainerColor = Color(0xFF2E7D32).copy(alpha = 0.2f),
                ),
            )
            FilterChip(
                selected = filter == CompatibilityStatus.MENU,
                onClick = { filter = CompatibilityStatus.MENU },
                label = { Text(stringResource(R.string.compatibility_filter_menu)) },
            )
            FilterChip(
                selected = filter == CompatibilityStatus.INTRO,
                onClick = { filter = CompatibilityStatus.INTRO },
                label = { Text(stringResource(R.string.compatibility_filter_intro)) },
            )
            FilterChip(
                selected = filter == CompatibilityStatus.NOTHING,
                onClick = { filter = CompatibilityStatus.NOTHING },
                label = { Text(stringResource(R.string.compatibility_filter_nothing)) },
                colors = FilterChipDefaults.filterChipColors(
                    selectedContainerColor = Color(0xFFC62828).copy(alpha = 0.2f),
                ),
            )
        }

        LazyColumn(
            contentPadding = androidx.compose.foundation.layout.PaddingValues(16.dp),
            verticalArrangement = Arrangement.spacedBy(8.dp),
        ) {
            items(games, key = { it.id }) { game ->
                Card(
                    onClick = { onGameClick(game.id) },
                    modifier = Modifier.fillMaxWidth(),
                    shape = MaterialTheme.shapes.medium,
                    colors = CardDefaults.cardColors(
                        containerColor = MaterialTheme.colorScheme.surfaceContainerHigh,
                    ),
                ) {
                    Row(
                        modifier = Modifier
                            .fillMaxWidth()
                            .padding(12.dp),
                        verticalAlignment = Alignment.CenterVertically,
                    ) {
                        Column(modifier = Modifier.weight(1f)) {
                            Text(
                                text = game.title,
                                style = MaterialTheme.typography.titleMedium,
                                fontWeight = FontWeight.SemiBold,
                            )
                            Text(
                                text = game.serial,
                                style = MaterialTheme.typography.bodySmall,
                                color = MaterialTheme.colorScheme.onSurfaceVariant,
                            )
                        }
                        CompatibilityBadge(status = game.compatibility)
                    }
                }
            }
        }
    }
}
