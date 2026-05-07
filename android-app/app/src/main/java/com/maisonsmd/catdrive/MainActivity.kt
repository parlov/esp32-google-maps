package com.maisonsmd.catdrive

import android.annotation.SuppressLint
import android.bluetooth.BluetoothDevice
import android.content.*
import android.content.SharedPreferences.OnSharedPreferenceChangeListener
import android.os.Bundle
import android.os.IBinder
import android.widget.Toast
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.lifecycle.ViewModelProvider
import androidx.localbroadcastmanager.content.LocalBroadcastManager
import androidx.navigation.findNavController
import androidx.navigation.ui.AppBarConfiguration
import androidx.navigation.ui.setupActionBarWithNavController
import androidx.navigation.ui.setupWithNavController
import com.google.android.material.bottomnavigation.BottomNavigationView
import com.maisonsmd.catdrive.databinding.ActivityMainBinding
import com.maisonsmd.catdrive.lib.Intents
import com.maisonsmd.catdrive.lib.NavigationData
import com.maisonsmd.catdrive.service.BleService
import com.maisonsmd.catdrive.ui.ActivityViewModel
import com.maisonsmd.catdrive.ui.BleDeviceSelectionActivity
import com.maisonsmd.catdrive.utils.PermissionCheck
import com.maisonsmd.catdrive.utils.ServiceManager
import timber.log.Timber

val SHARED_PREFERENCES_FILE = "${BuildConfig.APPLICATION_ID}.preferences"

class MainActivity : AppCompatActivity() {
    private lateinit var mBinding: ActivityMainBinding
    private lateinit var mViewModel: ActivityViewModel
    private var mNavigationService: GoogleMapNotificationListener? = null
    private var mNavigationServiceBound = false
    private var mBroadcastService: BleService? = null
    private var mBroadcastServiceBound = false
    private lateinit var mSharedPref: SharedPreferences

    @Deprecated("Deprecated in Java")
    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        super.onActivityResult(requestCode, resultCode, data)
        mBroadcastService?.connectToLastDevice()
    }

    override fun onRequestPermissionsResult(
        requestCode: Int,
        permissions: Array<out String>,
        grantResults: IntArray
    ) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        // notify settings fragment to update the permissions to view
        mViewModel.permissionUpdatedTimestamp.postValue(System.currentTimeMillis())
    }

    // Bind GoogleMapNotificationListener service
    private val navigationConnection = object : ServiceConnection {
        override fun onServiceConnected(name: ComponentName?, service: IBinder?) {
            if (service !is GoogleMapNotificationListener.LocalBinder) return

            mNavigationService = service.getService()
            mNavigationServiceBound = true

            Timber.d("$name connected")
            mViewModel.navigationData.postValue(mNavigationService!!.lastNavigationData)
        }

        override fun onServiceDisconnected(name: ComponentName?) {
            mNavigationService = null
            mNavigationServiceBound = false
            Timber.d("$name disconnected")
            mViewModel.navigationData.postValue(NavigationData())
        }
    }

    // Bind BroadcastService service
    private val broadcastConnection = object : ServiceConnection {
        override fun onServiceConnected(name: ComponentName?, service: IBinder?) {
            Timber.d("Service $name connected")
            if (service !is BleService.LocalBinder) return

            mBroadcastService = service.getService()
            mBroadcastServiceBound = true

            Timber.d("$name connected")
            mViewModel.connectedDevice.postValue(mBroadcastService!!.connectedDevice)
            mViewModel.serviceRunInBackground.postValue(mBroadcastService!!.runInBackground)
        }

        override fun onServiceDisconnected(name: ComponentName?) {
            mBroadcastService = null
            mNavigationServiceBound = false
            Timber.d("$name disconnected")
            mViewModel.navigationData.postValue(NavigationData())
            mViewModel.serviceRunInBackground.postValue(false)
        }
    }

    private val navigationReceiver: BroadcastReceiver = object : BroadcastReceiver() {
        override fun onReceive(context: Context?, intent: Intent) {
            val data = intent.getParcelableExtra("navigation_data") as NavigationData?
            mViewModel.navigationData.postValue(data)
        }
    }

    private val broadcastReceiver: BroadcastReceiver = object : BroadcastReceiver() {
        override fun onReceive(context: Context?, intent: Intent) {
            when (intent.action) {
                Intents.CONNECTION_UPDATE -> {
                    val connectedDevice = mBroadcastService?.connectedDevice
                    Timber.d("Connection update: ${intent.getStringExtra("status")} ${connectedDevice?.address}")
                    mViewModel.connectedDevice.postValue(connectedDevice)
                    connectedDevice?.let {
                        if (PermissionCheck.checkBluetoothConnectPermission(applicationContext)) {
                            with(mSharedPref.edit()) {
                                putString("last_device_name", it.name)
                                putString("last_device_address", it.address)
                                apply()
                            }
                        }
                        sendLastNavigationDataToDevice()
                    }
                }

                Intents.GPS_UPDATE -> mViewModel.speed.postValue(intent.getIntExtra("speed", 0))
                Intents.BACKGROUND_SERVICE_STATUS -> {
                    mViewModel.serviceRunInBackground.postValue(
                        intent.getBooleanExtra(
                            "run_in_background",
                            false
                        )
                    )
                }
            }
        }
    }

    private val sharedPreferenceListener =
        OnSharedPreferenceChangeListener { _, _ -> mBroadcastService?.sendPreferencesToDevice() }

    @SuppressLint("MissingPermission")
    private val mDeviceSelectionRequest = registerForActivityResult(
        ActivityResultContracts.StartActivityForResult()
    ) {
        if (it.resultCode == RESULT_OK) {
            val device = it.data!!.getParcelableExtra<BluetoothDevice>("device")!!
            if (PermissionCheck.checkBluetoothScanPermission(this)) {
                // Toast.makeText(this, "Connecting to ${device.name}...", Toast.LENGTH_SHORT).show()
                ServiceManager.requestConnectDevice(this, device)
            } else {
                Toast.makeText(
                    this,
                    "No bluetooth permission or BT not enabled!",
                    Toast.LENGTH_SHORT
                ).show()
            }
        }
    }

    private fun checkPermissions() {
        if (!PermissionCheck.allPermissionsGranted(this)) {
            Toast.makeText(
                this,
                "Some permissions are not granted, see Settings page",
                Toast.LENGTH_LONG
            ).show()
        }
    }

    fun openDeviceSelectionActivity() {
        mDeviceSelectionRequest.launch(
            Intent(
                applicationContext,
                BleDeviceSelectionActivity::class.java
            )
        )
    }

    private fun sendLastNavigationDataToDevice() {
        mBroadcastService?.sendToDevice(mNavigationService?.lastNavigationData)
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        Timber.plant(object : Timber.DebugTree() {
            override fun createStackElementTag(element: StackTraceElement): String {
                return super.createStackElementTag(element) + ":" + element.lineNumber + " " + element.methodName
            }
        })

        mBinding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(mBinding.root)

        val navView: BottomNavigationView = mBinding.navView

        val navController = findNavController(R.id.nav_host_fragment_activity_main)
        // Passing each menu ID as a set of Ids because each
        // menu should be considered as top level destinations.
        val appBarConfiguration = AppBarConfiguration(
            setOf(
                R.id.navigation_home, R.id.navigation_settings
            )
        )
        setupActionBarWithNavController(navController, appBarConfiguration)
        navView.setupWithNavController(navController)

        mViewModel = ViewModelProvider(this)[ActivityViewModel::class.java]
        mSharedPref = getSharedPreferences(SHARED_PREFERENCES_FILE, Context.MODE_PRIVATE)
    }

    override fun onStart() {
        super.onStart()
        checkPermissions()

        // Listen to GoogleMapNotificationListener dat
        LocalBroadcastManager.getInstance(this)
            .registerReceiver(navigationReceiver, IntentFilter(Intents.NAVIGATION_UPDATE))
        // Listen to BroadcastService data
        LocalBroadcastManager.getInstance(this)
            .registerReceiver(broadcastReceiver, IntentFilter().apply {
                addAction(Intents.GPS_UPDATE)
                addAction(Intents.CONNECTION_UPDATE)
                addAction(Intents.BACKGROUND_SERVICE_STATUS)
            })

        Intent(this, GoogleMapNotificationListener::class.java).also { intent ->
            intent.action = Intents.BIND_LOCAL_SERVICE
        }.also { intent -> bindService(intent, navigationConnection, Context.BIND_AUTO_CREATE) }
        Intent(this, BleService::class.java)
            .also { intent -> intent.action = Intents.BIND_LOCAL_SERVICE }
            .also { intent -> bindService(intent, broadcastConnection, Context.BIND_AUTO_CREATE) }

        mSharedPref.registerOnSharedPreferenceChangeListener(sharedPreferenceListener)
    }

    // Caution: opening device selection activity stops this main activity
    override fun onStop() {
        LocalBroadcastManager.getInstance(this).unregisterReceiver(navigationReceiver)
        LocalBroadcastManager.getInstance(this).unregisterReceiver(broadcastReceiver)

        Timber.i("onStop")

        if (mNavigationServiceBound) {
            mNavigationServiceBound = false
            mNavigationService = null
            unbindService(navigationConnection)
        }

        if (mBroadcastServiceBound) {
            mBroadcastServiceBound = false
            mBroadcastService = null
            unbindService(broadcastConnection)
        }

        super.onStop()
    }
}