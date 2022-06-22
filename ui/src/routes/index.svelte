
<script>
    import { variables } from '$lib/utils/variables';
	import { fade } from 'svelte/transition';
    import { Progress, Button, Spinner } from 'sveltestrap';
    import { toast } from '@zerodevx/svelte-toast';
    import { onMount } from 'svelte';

    // ******* SHOW LEVEL ******** //
    let level = undefined;
    async function getCurrentLevel() {
        const response = await fetch(`/api/level/current`).catch(error => { throw new Error(`${error})`); });
        if(!response.ok) {
            toast.push(`Error ${response.status} ${response.statusText}<br>Unable to request current level.`, variables.toast.error)
        }
        const value = await response.text();
        level = parseInt(value);
    }

    let refreshInterval
    onMount(async () => { 
        getCurrentLevel()
        clearInterval(refreshInterval)
        refreshInterval = setInterval(getCurrentLevel, 5000)
    });

    // ******* REPRESSUREIZE ******** //
    let isVisible = false;
    async function repressurizeTube() {
        isVisible = true;
        const response = await fetch(`/api/restore/pressure`).catch(error => { throw new Error(`${error})`); });
		if(!response.ok) {
            toast.push(`Error ${response.status} ${response.statusText}<br>Unable to repressurize tube.`, variables.toast.error)
        }
        isVisible = false;
    }
</script>

<svelte:head>
  <title>Sensor Status</title>
</svelte:head>

<div class="container">
    <div class="row">
        {#if level == undefined}
        <div class="col">Requesting current tank level, please wait...</div>
        {:else}
        <div class="col-sm-12">Current tank level:</div>
        <div class="col-sm-9">
            <Progress animated value={level} style="height: 5rem;">{level}%</Progress>
        </div>
        <div class="col-sm-3">
            <Button on:click={repressurizeTube} block style="height: 5rem;">
                {#if isVisible}<div out:fade={{ delay: 5000 }}><Spinner size="sm" type="grow" />&nbsp;</div>{/if}
                Repressurize
            </Button>
        </div>
        {/if}
    </div>
</div>
