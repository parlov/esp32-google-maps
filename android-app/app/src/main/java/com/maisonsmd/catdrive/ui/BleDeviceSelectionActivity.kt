package com.maisonsmd.catdrive.ui

import android.annotation.SuppressLint
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothManager
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanResult
import android.content.Context
import android.content.Intent
import android.os.Bundle
import android.os.Handler
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Button
import android.widget.FrameLayout
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import androidx.recyclerview.widget.RecyclerView
import com.maisonsmd.catdrive.R
import com.maisonsmd.catdrive.utils.PermissionCheck
import timber.log.Timber


class CustomBleAdapter(private val onSelectCallback: (BluetoothDevice) -> Unit) :
    RecyclerView.Adapter<CustomBleAdapter.ViewHolder>() {
    private var mDataSet: MutableList<BluetoothDevice> = mutableListOf()

    fun clear() {
        val size = mDataSet.size
        mDataSet.clear()
        notifyItemRangeRemoved(0, size)
    }

    fun addItem(device: BluetoothDevice) {
        if (contains(device))
            return

        mDataSet.add(device)
        notifyItemInserted(mDataSet.size - 1)
    }

    fun contains(device: BluetoothDevice): Boolean {
        return mDataSet.contains(device)
    }

    // Return the size of your dataset (invoked by the layout manager)
    override fun getItemCount() = mDataSet.size

    class ViewHolder(view: View) : RecyclerView.ViewHolder(view) {
        val txtDeviceName: TextView = view.findViewById(R.id.txtItemDeviceName)
        val txtDeviceAddress: TextView = view.findViewById(R.id.txtItemMacAddress)
        val frame: FrameLayout = view.findViewById(R.id.frame)
    }

    // Create new views (invoked by the layout manager)
    override fun onCreateViewHolder(viewGroup: ViewGroup, viewType: Int): ViewHolder {
        // Create a new view, which defines the UI of the list item
        val view = LayoutInflater.from(viewGroup.context)
            .inflate(R.layout.device_row_item, viewGroup, false)

        return ViewHolder(view)
    }

    // Replace the contents of a view (invoked by the layout manager)
    @SuppressLint("MissingPermission")
    override fun onBindViewHolder(viewHolder: ViewHolder, position: Int) {

        // Get element from your dataset at this position and replace the
        // contents of the view with that element
        viewHolder.txtDeviceName.text = mDataSet[position].name
        viewHolder.txtDeviceAddress.text = mDataSet[position].address

        viewHolder.frame.setOnClickListener { _ -> onSelectCallback(mDataSet[position]) }
    }
}

@SuppressLint("MissingPermission")
class BleDeviceSelectionActivity : AppCompatActivity() {
    private lateinit var mViewDeviceAdapter: CustomBleAdapter
    private var isScanning = false
    private lateinit var scanButton: Button
    private lateinit var mAdapter: BluetoothAdapter
    private val mCallback: ScanCallback = object : ScanCallback() {
        override fun onScanResult(callbackType: Int, result: ScanResult) {
            // Timber.d("onScanResult ${result.device.name}(${result.device.address})")
            super.onScanResult(callbackType, result)

            val btDevice = result.device
            //only scan Espressif devices
            if (btDevice.address.startsWith("7C:2C:67")) {
                mViewDeviceAdapter.addItem(btDevice)
            }
        }
    }

    private fun onDeviceSelected(device: BluetoothDevice) {
        Timber.d("onDeviceSelected: $device")
        mAdapter.bluetoothLeScanner.stopScan(mCallback)
        setResult(RESULT_OK, Intent().apply { putExtra("device", device) })
        finish()
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_ble_selection)

        supportActionBar?.setDisplayHomeAsUpEnabled(true)

        mViewDeviceAdapter = CustomBleAdapter(this::onDeviceSelected)
        findViewById<RecyclerView?>(R.id.viewBleDeviceList).apply { adapter = mViewDeviceAdapter }
        scanButton = findViewById<Button?>(R.id.btnScanBle).apply {
            setOnClickListener {
                if (PermissionCheck.isBluetoothEnabled(this@BleDeviceSelectionActivity)) {
                    getDeviceList()
                }
            }
        }

        if (PermissionCheck.isBluetoothEnabled(this)) {
            mAdapter =
                (applicationContext.getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager).adapter!!
            getDeviceList()
        } else {
            PermissionCheck.requestEnableBluetooth(this)
        }
    }

    private fun getDeviceList() {
        if (!PermissionCheck.checkBluetoothPermissions(this)) {
            Timber.e("No bluetooth permission")
            PermissionCheck.requestBluetoothAccessPermissions(this)
            return
        }

        mViewDeviceAdapter.clear()

        mAdapter.bluetoothLeScanner.startScan(mCallback)
        isScanning = true
        scanButton.isEnabled = false
        // Stop after 5s
        Handler().postDelayed({
            if (isScanning) {
                isScanning = false
                scanButton.isEnabled = true
                mAdapter.bluetoothLeScanner?.stopScan(mCallback)
            }
        }, 5000)
    }
}