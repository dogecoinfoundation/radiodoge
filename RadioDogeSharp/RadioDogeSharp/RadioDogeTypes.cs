namespace RadioDoge
{
    internal enum SerialCommandType
    {
        None,
        GetNodeAddress,
        SetNodeAddresses,
        Ping,
        Message,
        HardwareInfo = 0x3f, //Translates to sending '?'
        BroadcastMessage = 0x62, // Translates to sending 'b'
        DisplayControl = 0x64, // Translates to sending 'd'
        HostFormedPacket = 0x68, // Translates to sending 'h'
        MultipartPacket = 0x6D, // Translates to sending 'm'
        ResultCode = 0xFE
    }

    internal enum BroadCastType
    {
        HubAnnouncement,
        NodeAnnouncement,
    }

    internal enum DisplayType
    {
        StringDisplay,
        RadioDogeLogo,
        DogeAnimation,
        CoinAnimation,
        ReceivingCoins,
        SendingCoins,
    }

    internal enum DogeCommandType
    {
        Registration = 15,
        BroadcastReceived = 98,
        GetDogeAddress = 120,
        SendDogeAddress = 240,
        GetUTXOs = 111,
        SendUTXOs = 112,
        RequestBalance = 101,
        BalanceReceived = 202,
        DogeCommandSuccess = 1,
        DogeCommandFailure = 254,
        TransactionRequest = 170,
        TransactionResult = 171,
    }

    internal enum DogeResponseCode
    {
        Success,
        InvalidAddress,
        AlreadyRegistered,
        NotRegistered,
        InvalidPin,
        NoStoredUTXOS,
        WatchlistFailure,
    }

    internal enum RegistrationFunction
    {
        AddRegistration = 10,
        RemoveRegistration = 20,
        UpdatePin = 30
    }

    internal enum ModeType
    {
        LoRaSetup,
        SPV,
        Test,
        Quit,
        Invalid
    }

    internal enum TestFunctions
    {
        SendMultipartPacket,
        SendSinglePacket,
        SendCountingTest,
        SendBroadcastTest,
        DisplayTest,
        LibDogecoinTest,
    }

    internal enum SPVFunctions
    {
        StartSPV,
        StopSPV
    }
}
