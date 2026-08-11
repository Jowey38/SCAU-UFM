module scau_dflowfm_water_balance_bridge
   use, intrinsic :: iso_c_binding, only: c_double, c_int32_t, c_int64_t, c_sizeof
   implicit none
   private

   integer(c_int32_t), parameter :: ABI_VERSION = 1_c_int32_t
   integer(c_int64_t), parameter :: COMPONENTS_ALL = int(z'00000000000000ff', c_int64_t)

   type, bind(C) :: water_balance_v1
      integer(c_int32_t) :: abi_version
      integer(c_int32_t) :: struct_size
      integer(c_int64_t) :: valid_components
      real(c_double) :: current_time_seconds
      real(c_double) :: storage_m3
      real(c_double) :: volume_error_cumulative_m3
      real(c_double) :: boundary_in_m3
      real(c_double) :: boundary_out_m3
      real(c_double) :: lateral_1d_in_m3
      real(c_double) :: lateral_1d_out_m3
      real(c_double) :: lateral_2d_in_m3
      real(c_double) :: lateral_2d_out_m3
      real(c_double) :: source_in_m3
      real(c_double) :: source_out_m3
      real(c_double) :: qext_1d_in_m3
      real(c_double) :: qext_1d_out_m3
      real(c_double) :: qext_2d_in_m3
      real(c_double) :: qext_2d_out_m3
      real(c_double) :: rain_in_m3
      real(c_double) :: evaporation_out_m3
      real(c_double) :: groundwater_in_m3
      real(c_double) :: groundwater_out_m3
   end type water_balance_v1

   public :: dflowfm_get_water_balance_v1

contains

   integer(c_int32_t) function dflowfm_get_water_balance_v1(out, out_size) &
      bind(C, name='dflowfm_get_water_balance_v1') result(status)
      use m_flow, only: vol1tot, volerrcum, vinbndcum, voutbndcum, &
                        vinlatcum, voutlatcum, vinsrccum, voutsrccum, &
                        vinextcum, voutextcum, vinraincum, voutevacum, &
                        vingrwcum, voutgrwcum
      use m_flowtimes, only: time1

      type(water_balance_v1), intent(out) :: out
      integer(c_int32_t), value, intent(in) :: out_size
      type(water_balance_v1) :: expected

      if (out_size /= int(c_sizeof(expected), c_int32_t)) then
         status = 1_c_int32_t
         return
      end if

      out%abi_version = ABI_VERSION
      out%struct_size = int(c_sizeof(out), c_int32_t)
      out%valid_components = COMPONENTS_ALL
      out%current_time_seconds = time1
      out%storage_m3 = vol1tot
      out%volume_error_cumulative_m3 = volerrcum
      out%boundary_in_m3 = vinbndcum
      out%boundary_out_m3 = voutbndcum
      out%lateral_1d_in_m3 = vinlatcum(1)
      out%lateral_1d_out_m3 = voutlatcum(1)
      out%lateral_2d_in_m3 = vinlatcum(2)
      out%lateral_2d_out_m3 = voutlatcum(2)
      out%source_in_m3 = vinsrccum
      out%source_out_m3 = voutsrccum
      out%qext_1d_in_m3 = vinextcum(1)
      out%qext_1d_out_m3 = voutextcum(1)
      out%qext_2d_in_m3 = vinextcum(2)
      out%qext_2d_out_m3 = voutextcum(2)
      out%rain_in_m3 = vinraincum
      out%evaporation_out_m3 = voutevacum
      out%groundwater_in_m3 = vingrwcum
      out%groundwater_out_m3 = voutgrwcum
      status = 0_c_int32_t
   end function dflowfm_get_water_balance_v1

end module scau_dflowfm_water_balance_bridge
