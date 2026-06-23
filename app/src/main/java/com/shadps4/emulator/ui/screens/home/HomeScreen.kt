package com.shadps4.emulator.ui.screens.home

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.statusBars
import androidx.compose.foundation.layout.windowInsetsTopHeight
import androidx.compose.foundation.lazy.LazyRow
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.Add
import androidx.compose.material.icons.rounded.Folder
import androidx.compose.material.icons.rounded.PlayArrow
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import com.shadps4.emulator.R
import com.shadps4.emulator.data.model.SampleGames
import com.shadps4.emulator.ui.components.GameCard
import com.shadps4.emulator.ui.components.RecentGameRow

@Composable
fun HomeScreen(
    onGameClick: (String) -> Unit,
    onSeeAll: () -> Unit,
    onOpenGame: () -> Unit,
    onInstallPkg: () -> Unit,
    onScanLibrary: () -> Unit,
) {
    val recent = remember { SampleGames.recent }
    val allGames = remember { SampleGames.all }

    Column(
        modifier = Modifier
            .fillMaxSize()
            .verticalScroll(rememberScrollState()),
    ) {
        Spacer(Modifier.windowInsetsTopHeight(WindowInsets.statusBars))

        // Hero header
        HeroHeader()

        // Quick actions
        QuickActions(
            onOpenGame = onOpenGame,
            onInstallPkg = onInstallPkg,
            onScanLibrary = onScanLibrary,
        )

        // Recently played
        SectionHeader(
            title = stringResource(R.string.home_recent_games),
            actionText = null,
            onAction = null,
        )
        if (recent.isEmpty()) {
            EmptyRecentCard()
        } else {
            Column(
                modifier = Modifier.padding(horizontal = 16.dp),
                verticalArrangement = Arrangement.spacedBy(8.dp),
            ) {
                recent.take(4).forEach { game ->
                    RecentGameRow(game = game, onClick = { onGameClick(game.id) })
                }
            }
        }

        Spacer(Modifier.height(16.dp))

        // All games row
        SectionHeader(
            title = stringResource(R.string.library_title),
            actionText = stringResource(R.string.library_scan),
            onAction = onSeeAll,
        )
        LazyRow(
            contentPadding = PaddingValues(horizontal = 16.dp, vertical = 4.dp),
            horizontalArrangement = Arrangement.spacedBy(12.dp),
        ) {
            items(allGames, key = { it.id }) { game ->
                GameCard(
                    game = game,
                    onClick = { onGameClick(game.id) },
                    modifier = Modifier.size(width = 140.dp, height = 200.dp),
                )
            }
        }

        Spacer(Modifier.height(24.dp))
    }
}

@Composable
private fun HeroHeader() {
    Surface(
        modifier = Modifier
            .fillMaxWidth()
            .padding(16.dp),
        shape = MaterialTheme.shapes.extraLarge,
        color = MaterialTheme.colorScheme.primaryContainer,
    ) {
        Box(
            modifier = Modifier
                .fillMaxWidth()
                .padding(24.dp),
        ) {
            Column {
                Text(
                    text = stringResource(R.string.home_welcome),
                    style = MaterialTheme.typography.displaySmall,
                    color = MaterialTheme.colorScheme.onPrimaryContainer,
                    fontWeight = FontWeight.Bold,
                )
                Spacer(Modifier.height(4.dp))
                Text(
                    text = stringResource(R.string.home_subtitle),
                    style = MaterialTheme.typography.bodyLarge,
                    color = MaterialTheme.colorScheme.onPrimaryContainer,
                )
            }
        }
    }
}

@Composable
private fun QuickActions(
    onOpenGame: () -> Unit,
    onInstallPkg: () -> Unit,
    onScanLibrary: () -> Unit,
) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .padding(horizontal = 16.dp),
        horizontalArrangement = Arrangement.spacedBy(12.dp),
    ) {
        QuickActionCard(
            modifier = Modifier.weight(1f),
            icon = Icons.Rounded.PlayArrow,
            label = stringResource(R.string.home_action_open_game),
            onClick = onOpenGame,
        )
        QuickActionCard(
            modifier = Modifier.weight(1f),
            icon = Icons.Rounded.Add,
            label = stringResource(R.string.home_action_install_pkg),
            onClick = onInstallPkg,
        )
        QuickActionCard(
            modifier = Modifier.weight(1f),
            icon = Icons.Rounded.Folder,
            label = stringResource(R.string.home_action_scan_library),
            onClick = onScanLibrary,
        )
    }
}

@Composable
private fun QuickActionCard(
    icon: ImageVector,
    label: String,
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
) {
    Card(
        onClick = onClick,
        modifier = modifier,
        shape = MaterialTheme.shapes.large,
        colors = CardDefaults.cardColors(
            containerColor = MaterialTheme.colorScheme.surfaceContainerHigh,
        ),
    ) {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .padding(vertical = 16.dp, horizontal = 12.dp),
            horizontalAlignment = Alignment.CenterHorizontally,
        ) {
            Surface(
                color = MaterialTheme.colorScheme.secondaryContainer,
                shape = MaterialTheme.shapes.medium,
            ) {
                Icon(
                    imageVector = icon,
                    contentDescription = null,
                    tint = MaterialTheme.colorScheme.onSecondaryContainer,
                    modifier = Modifier
                        .padding(10.dp)
                        .size(24.dp),
                )
            }
            Spacer(Modifier.height(8.dp))
            Text(
                text = label,
                style = MaterialTheme.typography.labelMedium,
                color = MaterialTheme.colorScheme.onSurface,
                fontWeight = FontWeight.Medium,
                maxLines = 1,
            )
        }
    }
}

@Composable
private fun SectionHeader(
    title: String,
    actionText: String?,
    onAction: (() -> Unit)?,
) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .padding(horizontal = 16.dp, vertical = 12.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Text(
            text = title,
            style = MaterialTheme.typography.titleLarge,
            fontWeight = FontWeight.SemiBold,
            color = MaterialTheme.colorScheme.onSurface,
            modifier = Modifier.weight(1f),
        )
        if (actionText != null && onAction != null) {
            TextButton(onClick = onAction) {
                Text(text = actionText)
            }
        }
    }
}

@Composable
private fun EmptyRecentCard() {
    Card(
        modifier = Modifier
            .fillMaxWidth()
            .padding(horizontal = 16.dp),
        shape = MaterialTheme.shapes.large,
        colors = CardDefaults.cardColors(
            containerColor = MaterialTheme.colorScheme.surfaceContainerHigh,
        ),
    ) {
        Text(
            text = stringResource(R.string.home_no_recent),
            style = MaterialTheme.typography.bodyMedium,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
            modifier = Modifier.padding(16.dp),
        )
    }
}
