static UINT8 ADrvLst_Add(ADRV_LIST** headPtr, const ADRV_INSTANCE* drvData)
{
	ADRV_LIST* newLstItem;
	ADRV_LIST* curLstItem;
	
	curLstItem = ADrvLst_FindItem(*headPtr, drvData, NULL);
	if (curLstItem != NULL)
		return 0x01;	// already in the list
	
	newLstItem = (ADRV_LIST*)malloc(sizeof(ADRV_LIST));
	newLstItem->drvInst = drvData;
	newLstItem->next = NULL;
	if (*headPtr == NULL)
	{
		*headPtr = newLstItem;
	}
	else
	{
		curLstItem = *headPtr;
		while(curLstItem->next != NULL)
			curLstItem = curLstItem->next;
		curLstItem->next = newLstItem;
	}
	return 0x00;
}

static UINT8 ADrvLst_Remove(ADRV_LIST** headPtr, const ADRV_INSTANCE* drvData)
{
	ADRV_LIST* curLstItem;
	ADRV_LIST* lastLstItm;
	
	curLstItem = ADrvLst_FindItem(*headPtr, drvData, &lastLstItm);
	if (curLstItem == NULL)
		return 0xFF;	// not found
	
	if (lastLstItm == NULL)
		*headPtr = curLstItem->next;
	else
		lastLstItm->next = curLstItem->next;
	free(curLstItem);
	return 0x00;
}

static ADRV_LIST* ADrvLst_FindItem(ADRV_LIST* head, const ADRV_INSTANCE* drvData, ADRV_LIST** retLastItm)
{
	ADRV_LIST* curLstItem;
	ADRV_LIST* lastLstItm;
	
	lastLstItm = NULL;
	curLstItem = head;
	while(curLstItem != NULL)
	{
		if (curLstItem->drvInst == drvData)
			break;
		lastLstItm = curLstItem;
		curLstItem = curLstItem->next;
	}
	if (retLastItm != NULL)
		*retLastItm = lastLstItm;
	return curLstItem;
}

static UINT8 ADrvLst_Clear(ADRV_LIST** headPtr)
{
	ADRV_LIST* curLstItem;
	ADRV_LIST* lastLstItm;
	
	curLstItem = *headPtr;
	*headPtr = NULL;
	while(curLstItem != NULL)
	{
		lastLstItm = curLstItem;
		curLstItem = curLstItem->next;
		free(lastLstItm);
	}
	return 0x00;
}
